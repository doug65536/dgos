#include "pci.h"
#include "cpu/ioport.h"
#include "mutex.h"
#include "printk.h"
#include "string.h"
#include "cpu/apic.h"
#include "vector.h"
#include "numeric.h"
#include "inttypes.h"
#include "time.h"
#include "bootinfo.h"

#define PCI_DEBUG   1
#if PCI_DEBUG
#define PCI_TRACE(...) printk("pci: " __VA_ARGS__)
#else
#define PCI_TRACE(...) ((void)0)
#endif

struct pci_ecam_t;

__BEGIN_ANONYMOUS

class pci_config_rw {
public:
    // Read up to 32 bits from PCI config space.
    // Value must be entirely within a single 32 bit dword.
    virtual uint32_t read(pci_addr_t addr, size_t offset, size_t size) = 0;

    // Write an arbitrarily sized and aligned block of values
    virtual bool write(pci_addr_t addr, size_t offset,
                       void const *values, size_t size,
                       bool align32 = true) = 0;

    // Read an arbitrarily sized and aligned block of values
    virtual bool copy(pci_addr_t addr, void *dest,
                      size_t offset, size_t size) = 0;
};

// Legacy I/O port PCI configuration space accessor
class pci_config_pio : public pci_config_rw {
public:
    // pci_config_rw interface
    uint32_t read(pci_addr_t addr, size_t offset, size_t size) override final;
    bool write(pci_addr_t addr, size_t offset,
               void const *values, size_t size, bool align32) override final;
    bool copy(pci_addr_t addr, void *dest,
              size_t offset, size_t size) override final;

private:
    static size_t pioofs(pci_addr_t addr, size_t offset);
};

// Modern PCIe ECAM MMIO PCI configuration space accessor
class pci_config_mmio : public pci_config_rw {
    inline pci_ecam_t *find_ecam(int bus);
public:
    // pci_config_rw interface
    uint32_t read(pci_addr_t addr, size_t offset, size_t size) override final;
    bool write(pci_addr_t addr, size_t offset,
               void const *values, size_t size, bool align32) override final;
    bool copy(pci_addr_t addr, void *dest,
              size_t offset, size_t size) override final;

private:
    static size_t ecamofs(pci_addr_t addr, int bus_adj, size_t offset);
};

__END_ANONYMOUS

//
// ECAM

struct pci_ecam_t {
    uint64_t base;
    char volatile *mapping;
    uint16_t segment;
    uint8_t st_bus;
    uint8_t en_bus;
};

static ext::vector<pci_ecam_t> pci_ecam_list;

static pci_config_pio pci_pio_accessor;
static pci_config_mmio pci_mmio_accessor;

using pci_lock_type = ext::noirq_lock<ext::spinlock>;
using pci_scoped_lock = ext::unique_lock<pci_lock_type>;
static pci_lock_type pci_lock;
static pci_config_rw *pci_accessor = &pci_pio_accessor;

#define PCI_ADDR    0xCF8
#define PCI_DATA    0xCFC

//
// MSI

struct pci_msi_caps_hdr_t {
    uint8_t capability_id;
    uint8_t next_ptr;
    uint16_t msg_ctrl;
};

struct pci_msi32_t {
    uint32_t addr;
    uint16_t data;
    uint16_t rsvd;
} _packed;

struct pci_msi64_t {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint16_t data;
    uint16_t rsvd;
} _packed;

struct pci_msix64_t {
    uint64_t addr;
    uint32_t data;
    uint32_t ctrl;
};

// With per-vector masking
struct pci_msi32_pvm_t {
    pci_msi32_t base;
    uint32_t irq_mask;
    uint32_t irq_pending;
} _packed;

C_ASSERT(sizeof(pci_msi32_pvm_t) == 0x10);
C_ASSERT(offsetof(pci_msi32_pvm_t, irq_mask) == 0x08);

// With per-vector masking
struct pci_msi64_pvm_t {
    pci_msi64_t base;
    uint32_t irq_mask;
    uint32_t irq_pending;
} _packed;

C_ASSERT(sizeof(pci_msi64_pvm_t) == 0x14);
C_ASSERT(offsetof(pci_msi64_pvm_t, irq_mask) == 0x0C);

struct pci_msix_mappings {
    pci_msix64_t volatile *tbl;
    uint32_t volatile *pba;
    uint32_t tbl_sz;
    uint32_t pba_sz;
};

static ext::vector<ext::pair<pci_addr_t, pci_msix_mappings>> pcix_tables;

#define offset_of(type, member) \
    ((uintptr_t)&(((type*)0x10U)->member) - 0x10U)
#define size_of(type, member) \
    sizeof(((type*)0x10U)->member)

static char const *pci_device_class_text_lookup[] = {
    "No device class, must be old",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge Device",
    "Simple Communication Controllers",
    "Base System Peripherals",
    "Input Devices",
    "Docking Stations",
    "Processors",
    "Serial Bus Controllers",
    "Wireless Controllers",
    "Intelligent I/O Controllers",
    "Satellite Communication Controllers",
    "Encryption/Decryption Controllers",
    "Data Acquisition and Signal Processing Controllers"
};

//
// Legacy port I/O access

uint32_t pci_config_pio::read(pci_addr_t addr, size_t offset, size_t size)
{
    // Must be power of two
    assert_ispo2(size);

    // Must be naturally aligned
    assert((offset & -size) == offset);

    assert(offset < 256);

    // Field requested must begin and end in the same 32 bit range
    assert((offset & -4) == ((offset + size - 1) & -4));

    uint32_t pci_address = pioofs(addr, offset);

    pci_scoped_lock lock(pci_lock);

    outd(PCI_ADDR, pci_address);
    uint32_t data = ind(PCI_DATA);

    lock.unlock();

    // Shift right 8 bits for every byte of misalignment
    // to put the requested data at the LSB
    data >>= (offset & 3) << 3;

    // If the size is not the entire register, only keep the bits we want
    if (size != sizeof(uint32_t))
        data &= ~((uint32_t)-1 << (size << 3));

    return data;
}

bool pci_config_pio::write(pci_addr_t addr, size_t offset,
                           void const *values, size_t size, bool )
{
    // Validate
    if (!(offset < 256 && size <= 256 && offset + size <= 256))
        return false;

    // Pointer to input data
    char *p = (char*)values;


    uint32_t pci_address = (1 << 31) |
            (addr.bus() << 16) |
            (addr.slot() << 11) |
            (addr.func() << 8) |
            (offset & -4);

    pci_scoped_lock lock(pci_lock);

    while (size > 0) {
        // Choose an I/O size that will realign
        // and try to write 32 bits
        size_t io_size = sizeof(uint32_t) -
                (offset & (sizeof(uint32_t)-1));

        // Cap size to available data
        if (io_size > size)
            io_size = size;

        // Get written data into LSB
        uint32_t write = 0;
        memcpy(&write, p, io_size);
        p += io_size;

        // Calculate config space write address
        uint32_t write_addr = pci_address + offset;
        offset += io_size;
        size -= io_size;

        // Number of low bytes unaffected
        size_t byte_ofs = write_addr & 3;

        // Shift written data into position
        write <<= (byte_ofs * 8);

        // 32 bit align
        write_addr -= byte_ofs;

        uint32_t preserve_mask = 0;
        if (io_size != sizeof(uint32_t)) {
            // Mask with (io_size * 8) LSB set to 1
            preserve_mask = ~((uint32_t)-1 << (io_size * 8));
            // Mask with preserved bits one
            preserve_mask = ~0U ^ (preserve_mask << (byte_ofs * 8));
        }

        // Set address
        outd(PCI_ADDR, write_addr);

        if (preserve_mask) {
            // Merge data with existing data
            uint32_t read = ind(PCI_DATA);
            write |= read & preserve_mask;
        }

        // Write 32 bits
        outd(PCI_DATA, write);
    }

    return true;
}

bool pci_config_pio::copy(pci_addr_t addr, void *dest,
                          size_t offset, size_t size)
{
    uint32_t value;
    char *out = (char*)dest;

    size_t block_sz;
    for (size_t i = 0; i < size; i += block_sz) {
        block_sz = ext::min(sizeof(uint32_t), size - i);
        value = pci_config_read(addr, offset + i, block_sz);

        memcpy(out + i, &value, block_sz);
    }

    return true;
}

size_t pci_config_pio::pioofs(pci_addr_t addr, size_t offset)
{
    return (1U << 31) |
            (addr.bus() << 16) |
            (addr.slot() << 11) |
            (addr.func() << 8) |
            (offset & -4);
}

pci_ecam_t *pci_config_mmio::find_ecam(int bus)
{
    size_t i, e;
    pci_ecam_t *ent = pci_ecam_list.data();
    for (i = 0, e = pci_ecam_list.size(); i < e; ++i, ++ent) {
        if (ent->st_bus <= bus && ent->en_bus > bus)
            return ent;
    }

    return nullptr;
}

uint32_t pci_config_mmio::read(pci_addr_t addr, size_t offset, size_t size)
{
    assert((offset & -size) == offset);

    int bus = addr.bus();

    pci_ecam_t *ent = find_ecam(bus);

    if (unlikely(!ent))
        return uint32_t(-1);

    uint64_t ecam_offset = ecamofs(addr, ent->st_bus, offset);

    if (likely(size == sizeof(uint32_t)))
        return mm_rd(*(uint32_t*)(ent->mapping + ecam_offset));

    if (likely(size == sizeof(uint16_t)))
        return mm_rd(*(uint16_t*)(ent->mapping + ecam_offset));

    if (likely(size == sizeof(uint8_t)))
        return mm_rd(*(uint8_t*)(ent->mapping + ecam_offset));

    panic("Nonsense operand size");
}

bool pci_config_mmio::write(pci_addr_t addr, size_t offset,
                            void const *values, size_t size, bool align32)
{
    int bus = addr.bus();

    pci_ecam_t *ent = find_ecam(bus);

    if (unlikely(!ent))
        return false;

    uint64_t ecam_offset = ecamofs(addr, ent->st_bus, offset);

    if (align32 && size < sizeof(uint32_t)) {
        // Round down to 32 bit boundary
        size_t word32_aligned_offset = ecam_offset & -4;

        // Calculate offset from 32 bit boundary
        size_t misalignment = ecam_offset - word32_aligned_offset;

        // Convert byte offset/size to bits
        uint8_t misalignment_bits = misalignment * 8;
        uint8_t value_bits = size * 8;

        // Make a mask with (value_bits) least signficant bits set to 1
        uint32_t value_mask = ~-(UINT32_C(1) << value_bits);

        // Figure out which bits we keep
        uint32_t and_mask = ~(value_mask << misalignment_bits);

        // Figure out which bits we set
        uint32_t or_mask = 0;
        memcpy((char*)&or_mask, values, size);
        or_mask <<= misalignment_bits;

        // Read, modify, write the old 32 bit value
        uint32_t value = mm_rd(*(uint32_t volatile *)
                               (ent->mapping + word32_aligned_offset));
        value &= and_mask;
        value |= or_mask;
        mm_wr(*(uint32_t volatile *)
              (ent->mapping + word32_aligned_offset), value);

        return true;
    }

    uint64_t value = 0;

    switch (size) {
    case 1:
        memcpy(&value, values, size);
        mm_wr(*(uint8_t volatile *)(ent->mapping + ecam_offset), value);
        return true;

    case 2:
        memcpy(&value, values, size);
        assert((ecam_offset & -2) == ecam_offset);
        mm_wr(*(uint16_t volatile*)(ent->mapping + ecam_offset), value);
        return true;

    case 4:
        assert((ecam_offset & -4) == ecam_offset);
        memcpy(&value, values, size);
        mm_wr(*(uint32_t volatile *)(ent->mapping + ecam_offset), value);
        return true;

    case 8:
        assert((ecam_offset & -8) == ecam_offset);
        memcpy(&value, values, size);
        mm_wr(*(uint64_t volatile *)(ent->mapping + ecam_offset), value);
        return true;

    default:
        mm_copy_wr(ent->mapping + ecam_offset, values, size);
        return true;
    }

    return true;
}

bool pci_config_mmio::copy(pci_addr_t addr, void *dest,
                           size_t offset, size_t size)
{
    int bus = addr.bus();

    pci_ecam_t *ent = find_ecam(bus);

    if (unlikely(!ent))
        return false;

    uint64_t ecam_offset = ecamofs(addr, ent->st_bus, offset);

    uint32_t value;
    switch (size) {
    case sizeof(uint8_t):
        value = mm_rd(*(uint8_t volatile const *)
                      (ent->mapping + ecam_offset));
        memcpy(dest, &value, sizeof(uint8_t));
        return true;
    case sizeof(uint16_t):
        value = mm_rd(*(uint16_t volatile const *)
                      (ent->mapping + ecam_offset));
        memcpy(dest, &value, sizeof(uint16_t));
        return true;
    case sizeof(uint32_t):
        value = mm_rd(*(uint32_t volatile const *)
                      (ent->mapping + ecam_offset));
        memcpy(dest, &value, sizeof(uint32_t));
        return true;
    }

    mm_copy_rd(dest, ent->mapping + ecam_offset, size);
    return true;
}

size_t pci_config_mmio::ecamofs(pci_addr_t addr, int bus_adj, size_t offset)
{
    return ((addr.bus() - bus_adj) << 20) +
            (addr.slot() << 15) +
            (addr.func() << 12) +
            unsigned(offset);
}

uint32_t pci_config_read(pci_addr_t addr, int offset, int size)
{
    return pci_accessor->read(addr, offset, size);
}

bool pci_config_write(pci_addr_t addr, size_t offset,
                             void *values, size_t size)
{
    return pci_accessor->write(addr, offset, values, size);
}

void pci_config_copy(pci_addr_t addr, void *dest, int ofs, size_t size)
{
//    PCI_TRACE("copy: bus=%#.2x, slot=%#.2x, func=%#.2x, ofs=%#.x, sz=%#.zx\n",
//              addr.bus(), addr.slot(), addr.func(), ofs, size);

    pci_accessor->copy(addr, dest, ofs, size);

    size_t i = 0;
    for (i = 0; i < size; ++i)
        if (((uint8_t const *)dest)[i] != 0xFF)
            break;
    if (i != size)
        hex_dump(dest, size);
//    else
//        PCI_TRACE("All 0xFF\n");
}

static void pci_enumerate_read(pci_addr_t addr, pci_config_hdr_t *config)
{
    pci_config_copy(addr, config, 0, sizeof(pci_config_hdr_t));
}

static int pci_enumerate_is_match(pci_dev_iterator_t const *iter,
                                  pci_dev_iterator_t const *blueprint = nullptr)
{
    if (blueprint == nullptr)
        return true;

    return (blueprint->dev_class == -1 ||
            blueprint->dev_class == iter->config.dev_class) &&
            (blueprint->subclass == -1 ||
             blueprint->subclass == iter->config.subclass) &&
            (blueprint->vendor == -1 ||
             blueprint->vendor == iter->config.vendor) &&
            (blueprint->device == -1 ||
             blueprint->device == iter->config.device);
}

static int pci_enumerate_next_direct(pci_dev_iterator_t *iter)
{
    for (;;) {
        // Skip over the function scan if header type not multifunction
        if (iter->func == 0 && !(iter->config.header_type & 0x80))
            iter->func = 7;

        ++iter->func;

        if (((iter->header_type & 0x80) && iter->func < 8) ||
                iter->func < 1) {
            // Might be a device here
            iter->addr = pci_addr_t(iter->segment, iter->bus,
                                    iter->slot, iter->func);
            pci_enumerate_read(iter->addr, &iter->config);

            // Capture header type on function 0
            if (iter->func == 0)
                iter->header_type = iter->config.header_type;

            // If device is not there, then next
            if (iter->config.dev_class == (uint8_t)~0 &&
                    iter->config.vendor == (uint16_t)~0)
                continue;

            //printdbg("pci enumerator: encountered vendor=%#.4x, device=%#.4x"
            //         ", class=%#.4x, subclass=%#.4x\n",
            //         iter->config.vendor, iter->config.device,
            //         iter->config.dev_class, iter->config.subclass);

            // If device is bridge, add bus to todo list
            if (iter->config.dev_class == PCI_DEV_CLASS_BRIDGE &&
                    iter->config.subclass == PCI_SUBCLASS_BRIDGE_PCI2PCI) {
                if (iter->bus_todo_len < countof(iter->bus_todo)) {
                     uint8_t secondary_bus =
                             (iter->config.base_addr[2] >> 8) & 0xFF;
                     iter->bus_todo[iter->bus_todo_len++] = secondary_bus;
                } else {
                    printdbg("Too many PCI bridges! Dropped one\n");
                }
            }

            // If device matched, return true
            if (pci_enumerate_is_match(iter))
                return 1;

            continue;
        }

        // Done with slot, carry to next slot
        iter->func = -1;
        if (++iter->slot >= 32) {
            // Ran out of slots, carry to next bus
            iter->slot = 0;

            if (iter->bus_todo_len > 0) {
                // Pop bridge bus off of the todo list
                iter->bus = iter->bus_todo[--iter->bus_todo_len];
            } else {
                // Ran out of busses, done
                return 0;
            }
        }
    }
}

static void pci_dev_iter_init(
        pci_dev_iterator_t *iter,
        int dev_class, int device, int vendor, int subclass)
{
    iter->reset();

    iter->dev_class = dev_class;
    iter->subclass = subclass;
    iter->vendor = vendor;
    iter->device = device;

    iter->bus = 0;
    iter->slot = 0;
    iter->func = -1;

    iter->header_type = 0;

    iter->bus_todo_len = 0;
}

static int pci_enumerate_begin_direct(
        pci_dev_iterator_t *iter,
        int dev_class = -1, int subclass = -1,
        int vendor = -1, int device = -1)
{
    pci_dev_iter_init(iter, dev_class, device, vendor, subclass);

    int found;

    while ((found = pci_enumerate_next_direct(iter)) != 0) {
        if (pci_enumerate_is_match(iter))
            break;
    }

    return found;
}

static pci_cache_t pci_cache;

int pci_init(void)
{
    // Initialize every BAR
    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin_direct(&pci_iter))
        return 0;

    printk("Caching PCI device list\n");

    do {
        printk("Found PCI device (%s)\n"
               " bus=%#.4x"
               ", slot=%#.2x"
               ", func=%#.2x"
               ", vendor=%#.4x"
               ", device=%#.4x"
               ", class=%#.2x"
               ", subclass=%#.2x"
               ", progif=%#.2x\n",
               pci_describe_device_iter(pci_iter),
               pci_iter.bus,
               pci_iter.slot,
               pci_iter.func,
               pci_iter.config.vendor,
               pci_iter.config.device,
               pci_iter.config.dev_class,
               pci_iter.config.subclass,
               pci_iter.config.prog_if);

        if (unlikely(!pci_cache.iters.push_back(pci_iter)))
            panic_oom();
    } while (likely(pci_enumerate_next_direct(&pci_iter)));

    printk("Enumerating PCI Express capabilities\n");

    struct pcie_cap_t {
        uint8_t id;
        uint8_t next;
        uint16_t pcie_caps;
        uint32_t dev_caps;
        uint16_t dev_ctrl;
        uint16_t dev_status;
        uint32_t link_caps;
        uint16_t link_ctrl;
        uint16_t link_status;
        uint32_t slot_caps;
        uint16_t slot_ctrl;
        uint16_t slot_status;
        uint16_t root_ctrl;
        uint16_t root_caps;
        uint32_t root_status;

        uint32_t dev_caps2;
        uint16_t dev_ctrl2;
        uint16_t dev_status2;
        uint32_t link_caps2;
        uint16_t link_ctrl2;
        uint16_t link_status2;
        uint32_t slot_caps2;
        uint16_t slot_ctrl2;
        uint16_t slot_status2;
    };

    C_ASSERT(sizeof(pcie_cap_t) == 0x3C);

    for (pci_dev_iterator_t const& it : pci_cache.iters) {
        int ofs = pci_find_capability(it.addr, PCICAP_PCIE);

        if (ofs) {
            pcie_cap_t pcie_cap;

            pci_config_copy(it.addr, &pcie_cap, ofs, sizeof(pcie_cap));

            printk("Found PCIe capability on %#.2x:%#.2x:%#.2x\n",
                   it.addr.bus(), it.addr.slot(), it.addr.func());
        } else {
            printk("No PCIe capability on %.2x:%.2x:%.2x\n",
                   it.addr.bus(), it.addr.slot(), it.addr.func());
        }
    }

    printk("Autoconfiguring PCI BARs\n");

    for (pci_dev_iterator_t& pci_iter : pci_cache.iters) {
        char const *description = pci_describe_device_iter(pci_iter);

        printk("Probing %d:%d:%d (%s)\n",
               pci_iter.bus, pci_iter.slot, pci_iter.func,
               description);

        bool any_mmio = false;
        bool any_io = false;
        bool is_64 = false;

        for (int_fast8_t bar = 0; bar < 6; bar += is_64 ? 2 : 1) {
            is_64 = pci_iter.config.is_bar_64bit(bar);

            // Save original base
            uint64_t orig = pci_iter.config.get_bar(bar);

            if (!pci_iter.config.is_bar_mmio(bar)) {
                any_io = true;
            } else {
                any_mmio = true;

                // Write all ones
                pci_iter.config.set_mmio_bar(pci_iter, bar, ~0U);

                // Get readback value
                uint64_t readback = pci_iter.config.get_bar(bar);

                // Restore original value
                pci_iter.config.set_mmio_bar(pci_iter, bar, orig);

                // Keep info
                //uint_fast8_t info = readback & ~-16;

                // Mask off information bits 3:0
                readback &= -16;

                // Ignore BAR if it resulted in zero
                if (!readback || (orig & -16))
                    continue;

                // Get size
                uint_fast8_t log2_size = bit_lsb_set(readback & -16);
                uint64_t size = UINT64_C(1) << log2_size;

                // Allocate twice the needed size to align
                uint64_t newbase = mm_alloc_hole(size * 2);
                uint64_t newend = newbase + size * 2;

                // Calculate aligned range to use
                uint64_t used_st = (newbase + size - 1) & -size;
                uint64_t used_en = used_st + size;

                // Compute unused portion at beginning of allocation
                uint64_t unused_lo_st = newbase;
                uint64_t unused_lo_en = used_st;
                uint64_t unused_lo_sz = unused_lo_en - unused_lo_st;

                // Compute unused portion at end of allocation
                uint64_t unused_hi_st = used_en;
                uint64_t unused_hi_en = newend;
                uint64_t unused_hi_sz = unused_hi_en - unused_hi_st;

                // Return unused beginning portion to allocator
                if (unused_lo_sz)
                    mm_free_hole(unused_lo_st, unused_lo_sz);

                // Return unused end portion to allocator
                if (unused_hi_sz)
                    mm_free_hole(unused_hi_st, unused_hi_sz);

                // Set the BAR
                pci_iter.config.set_mmio_bar(pci_iter, bar, used_st);

                printk("...assigned %d:%d:%d BAR[%d] to MMIO"
                          "=%#" PRIx64 "-%#" PRIx64 "\n",
                          pci_iter.bus, pci_iter.slot, pci_iter.func, bar,
                          used_st, used_en-1);
            }
        }

        pci_adj_control_bits(pci_iter, (any_mmio ? PCI_CMD_MSE : 0) |
                             (any_io ? PCI_CMD_IOSE : 0) | PCI_CMD_BME,
                             (!any_mmio ? PCI_CMD_MSE : 0) |
                             (!any_io ? PCI_CMD_IOSE : 0));
    }

    pci_cache.updated_at = time_ns();

    return 0;
}

static uint64_t pci_setup_bar(pci_addr_t addr, int bir)
{
    uint64_t bar;
    uint32_t bar_ofs = offsetof(pci_config_hdr_t, base_addr) +
            bir * sizeof(uint32_t);

    pci_config_copy(addr, &bar, bar_ofs, sizeof(bar));

    bool use32 = (PCI_BAR_MMIO_TYPE_GET(bar) == 0);

    if (use32)
        bar &= 0xFFFFFFFF;

    size_t bar_width = use32 ? sizeof(uint32_t) : sizeof(uint64_t);

    // Autodetect address space needed by writing all bits one in BAR
    uint64_t new_bar = bar;
    PCI_BAR_MMIO_BA_SET(new_bar, PCI_BAR_MMIO_BA_MASK);
    pci_config_write(addr, bar_ofs, &new_bar, bar_width);

    // Read back BAR, size needed is indicated by number of LSB zero bits
    pci_config_copy(addr, &new_bar, bar_ofs, bar_width);

    uint8_t log2_sz = bit_lsb_set_64(new_bar & PCI_BAR_MMIO_BA_MASK);

    uint32_t alloc_sz = 1U << log2_sz;

    // Allocate twice as much to align (unused portion(s) freed later)
    uint32_t bar_alloc = mm_alloc_hole(alloc_sz << 1);

    // Naturally align
    uint32_t aligned_addr = (bar_alloc + alloc_sz) & -alloc_sz;

    // Update base address
    PCI_BAR_MMIO_BA_SET(new_bar, aligned_addr);

    // Give back extra unused space that may open up after alignment

    uint32_t unused_before = aligned_addr - bar_alloc;
    uint32_t kept_end = aligned_addr + alloc_sz;
    uint32_t alloc_end = bar_alloc + (alloc_sz << 1);
    uint32_t unused_after = alloc_end - kept_end;

    if (unused_before)
        mm_free_hole(bar_alloc, unused_before);

    if (unused_after)
        mm_free_hole(kept_end, unused_after);

    // Write back BAR
    pci_config_write(addr, bar_ofs, &new_bar, bar_width);

    return bar_alloc;
}

static int pci_enum_capabilities_match(
        uint8_t id, int ofs, uintptr_t context)
{
    if (id == context)
        return ofs;
    return 0;
}

int pci_enum_capabilities(int start, pci_addr_t addr,
                          int (*callback)(uint8_t, int, uintptr_t),
                          uintptr_t context)
{
    int ofs;

    if (start == 0) {
        // If not continuing a scan for capabilities, make sure the
        // status register indicates that a capability list exists
        int status = pci_config_read(
                    addr, offsetof(pci_config_hdr_t, status),
                    sizeof(uint16_t));

        if (!(status & PCI_CFG_STATUS_CAPLIST))
            return 0;

        // Read the offset of the first capability
        start = offsetof(pci_config_hdr_t, capabilities_ptr);

        ofs = pci_config_read(addr, start, 1);
    } else {
        // Read next link
        ofs = pci_config_read(addr, start + 1, 1);
    }

    while (ofs != 0) {
        uint16_t type_next = pci_config_read(addr, ofs, sizeof(uint16_t));
        uint8_t type = type_next & 0xFF;
        uint8_t next = (type_next >> 8) & 0xFF;

        PCI_TRACE("cap: ofs=%#x, bus=%d, func=%d, type=%#x, next=%#x\n",
                  ofs, addr.bus(), addr.func(), type, next);

        int result = callback(type, ofs, context);
        if (result != 0)
            return result;

        ofs = next;
    }

    //
    // Scan extended capabilities

//    if (!pci_ecam_list.empty()) {
//        ofs = 0x100;

//        while (ofs != 0 && unsigned(ofs) < 4096 - sizeof(int32_t)) {
//            uint32_t extcap = pci_config_read(addr, ofs, sizeof(uint32_t));
//            uint16_t capability_id = PCI_MSIX_EXTCAP_CAPID_GET(extcap);
//            //uint16_t capability_ver = PCI_MSIX_EXTCAP_CAPVER_GET(extcap);
//            uint16_t nextofs = PCI_MSIX_EXTCAP_NEXTOFS_GET(extcap);

//            int result = callback(capability_id, ofs, context);
//            if (result != 0)
//                return result;

//            // Software must mask the low two bits
//            ofs = nextofs & -4;
//        }
//    }

    return 0;
}

int pci_find_capability(pci_addr_t addr, int capability_id, int start)
{
    return pci_enum_capabilities(start, addr, pci_enum_capabilities_match,
                                 capability_id);
}

bool pci_try_msi_irq(pci_dev_iterator_t const& pci_dev,
                     pci_irq_range_t *irq_range,
                     int cpu, bool distribute, int req_count,
                     intr_handler_t handler, char const *name,
                     int const *target_cpus,
                     int const *vector_offsets)
{
    // Assume we can't use MSI at first, prepare to use pin interrupt
    irq_range->base = pci_dev.config.irq_line;
    irq_range->count = 1;
    irq_range->msix = false;

    bool use_msi = pci_set_msi_irq(pci_dev, irq_range, cpu,
                                   distribute, req_count, handler, name,
                                   target_cpus, vector_offsets);

    if (!use_msi) {
        // Plain IRQ pin
        PCI_TRACE("Using pin IRQ for IRQ %d\n", irq_range->base);

        //pci_set_irq_pin(pci_dev.addr, pci_dev.config.irq_pin);
        //pci_set_irq_line(pci_dev.addr, pci_dev.config.irq_line);

        irq_hook(pci_dev.config.irq_line, handler, name);
        irq_setmask(pci_dev.config.irq_line, true);
    }

    return use_msi;
}

static int pci_find_msi_msix(pci_addr_t addr,
                             bool& msix, pci_msi_caps_hdr_t& caps)
{

    // Look for the MSI-X extended capability
    int capability = 0;

    if (likely(bootinfo_parameter(bootparam_t::msix_enable)))
        capability = pci_find_capability(addr, PCICAP_MSIX);

    if (capability) {
        msix = true;
        PCI_TRACE("Found MSI-X capability for PCI %u:%u:%u\n",
                  addr.bus(), addr.slot(), addr.func());
    } else {
        // Fall back to MSI
        msix = false;

        if (likely(bootinfo_parameter(bootparam_t::msi_enable)))
            capability = pci_find_capability(addr, PCICAP_MSI);

        if (capability) {
            PCI_TRACE("Found MSI capability for PCI %u:%u:%u\n",
                      addr.bus(), addr.slot(), addr.func());
        }
    }

    if (capability) {
        // Read the header
        pci_config_copy(addr, &caps, capability, sizeof(caps));
    } else {
        PCI_TRACE("No MSI/MSI-X capability (or disabled) for PCI %u:%u:%u\n",
                  addr.bus(), addr.slot(), addr.func());
        return 0;
    }

    return capability;
}

// Infer the number of vectors needed from the highest vector offset
int pci_vector_count_from_offsets(int const *vector_offsets, int count)
{
    return ext::accumulate(vector_offsets, vector_offsets + count, 0,
                      [&](int highest, int const& item) {
        return ext::max(highest, item);
    }) + 1;
}

// Returns with the function masked if possible
// Use pci_set_irq_mask to unmask it when appropriate
bool pci_set_msi_irq(pci_dev_iterator_t const& pci_dev,
                            pci_irq_range_t *irq_range, int cpu,
                            bool distribute, size_t req_count,
                            intr_handler_t handler, char const *name,
                            int const *target_cpus, int const *vector_offsets)
{
    pci_addr_t addr = pci_dev;

    bool msix = false;
    pci_msi_caps_hdr_t caps{};
    int capability = pci_find_msi_msix(addr, msix, caps);

    if (!capability)
        return false;

    if (msix) {
        // 6.8.3.2 MSI-X configuration

        // "Software must not modify the MSI-X table when any IRQ is unmasked",
        // so we mask the whole function
        PCI_MSIX_MSG_CTRL_MASK_SET(caps.msg_ctrl, 1);

        // Enable MSI-X
        PCI_MSIX_MSG_CTRL_EN_SET(caps.msg_ctrl, 1);

        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));

        size_t table_count = PCI_MSIX_MSG_CTRL_TBLSZ_GET(caps.msg_ctrl) + 1;

        uint32_t tbl_pba[2];
        pci_config_copy(addr, tbl_pba,
                        capability + PCI_MSIX_TBL, sizeof(tbl_pba));

        // See which BIR it uses initially
        int tbl_bir = PCI_MSIX_TBL_BIR_GET(tbl_pba[0]);
        uint32_t tbl_ofs = tbl_pba[0] & -8;

        int pba_bir = PCI_MSIX_TBL_BIR_GET(tbl_pba[1]);
        uint32_t pba_ofs = tbl_pba[1] & -8;

        uint32_t tbl_sz = sizeof(pci_msix64_t) * table_count;
        // Round up to a multiple of 64 bits and scale down to byte count
        uint32_t pba_sz = ((table_count + 63) & -64) >> 3;

        uint64_t tbl_base = 0;
        uint64_t pba_base = 0;

        tbl_base = pci_dev.config.get_bar(tbl_bir);

        // Read the BAR indicated by the table BIR
//        pci_config_copy(addr, &tbl_base,
//                        offsetof(pci_config_hdr_t, base_addr) +
//                        sizeof(uint32_t) * tbl_bir, sizeof(tbl_base));

        // Read the BAR indicated by the PBA BIR
        pba_base = pci_dev.config.get_bar(pba_base);

//        pci_config_copy(addr, &pba_base,
//                        offsetof(pci_config_hdr_t, base_addr) +
//                        sizeof(uint32_t) * pba_bir, sizeof(pba_base));

       // Handle uninitialized table BAR

        if (unlikely(tbl_base == 0)) {
            tbl_base = pci_setup_bar(addr, tbl_bir);

            if (tbl_bir == pba_bir)
                pba_base = tbl_base;
        }

        // Handle uninitialized PBA BAR
        if (unlikely(pba_base == 0 && tbl_bir != pba_bir))
            pba_base = pci_setup_bar(addr, pba_bir);

        tbl_base += tbl_ofs;
        pba_base += pba_ofs;

        pci_msix64_t volatile *tbl = (pci_msix64_t*)
                mmap((void*)(tbl_base & -16), tbl_sz, PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU);

        uint32_t volatile *pba = (uint32_t*)
                mmap((void*)(pba_base & -16), pba_sz, PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU);

        if (unlikely(!pcix_tables.emplace_back(addr, pci_msix_mappings{
                                               tbl, pba, tbl_sz, pba_sz })))
            panic_oom();

        size_t tbl_cnt = ext::min(req_count, table_count);

        ext::vector<msi_irq_mem_t> msi_writes(tbl_cnt);

        if (vector_offsets) {
            irq_range->count = pci_vector_count_from_offsets(
                        vector_offsets, tbl_cnt);
        } else {
            irq_range->count = tbl_cnt;
        }
        irq_range->msix = true;
        irq_range->base = apic_msi_irq_alloc(
                    msi_writes.data(), tbl_cnt, cpu, distribute,
                    handler, name, target_cpus, vector_offsets, false);

        if (irq_range->base == 0)
            return false;

        PCI_TRACE("Allocated MSI-X IRQ %d-%d\n",
                  irq_range->base, irq_range->base + irq_range->count - 1);

        size_t i;
        size_t n = 0;
        for (i = 0; i != table_count; ++i) {
            msi_irq_mem_t const& write = msi_writes[n++];
            n *= (n < tbl_cnt);
            tbl[i].addr = write.addr;
            tbl[i].data = write.data;
            // 0 = Not masked
            PCI_MSIX_VEC_CTL_MASKIRQ_SET(tbl[i].ctrl, 0);
        }

        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));
    } else {
        irq_range->msix = false;

        // Extract the multi message capability value
        uint8_t multi_exp = PCI_MSI_MSG_CTRL_MMC_GET(caps.msg_ctrl);
        uint8_t multi_cap = 1U << multi_exp;

        // Sanity check multi message capability
        assert(multi_cap <= 32);

        // Limit it to highest documented value (as at PCI v2.2)
        if (unlikely(multi_cap > 32))
            multi_cap = 32;

        uint8_t multi_en = req_count ? multi_exp : 0;

        PCI_MSI_MSG_CTRL_EN_SET(caps.msg_ctrl, 1);
        PCI_MSI_MSG_CTRL_MME_SET(caps.msg_ctrl, multi_en);

        // Allocate IRQs
        msi_irq_mem_t mem[32];
        irq_range->count = 1 << multi_en;
        irq_range->msix = false;
        irq_range->base = apic_msi_irq_alloc(
                    mem, irq_range->count,
                    cpu, distribute, handler, name,
                    nullptr, nullptr, true);

        PCI_TRACE("Allocated MSI IRQ %d-%d\n",
                  irq_range->base, irq_range->base + irq_range->count - 1);

        // Use 32-bit or 64-bit according to capability
        if (caps.msg_ctrl & PCI_MSI_MSG_CTRL_CAP64) {
            // 64 bit address
            PCI_TRACE("Writing %d-bit MSI config\n", 64);

            pci_msi64_t cfg;
            cfg.addr_lo = (uint32_t)mem[0].addr;
            cfg.addr_hi = (uint32_t)(mem[0].addr >> 32);
            cfg.data = (uint16_t)mem[0].data;

            pci_config_write(addr, capability + sizeof(caps),
                             &cfg, sizeof(cfg));
        } else {
            // 32 bit address
            PCI_TRACE("Writing %d-bit MSI config\n", 32);

            pci_msi32_t cfg;
            cfg.addr = (uint32_t)mem[0].addr;
            cfg.data = (uint16_t)mem[0].data;

            pci_config_write(addr, capability + sizeof(caps),
                             &cfg, sizeof(cfg));
        }

        // Write msg_ctrl
        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));
    }

    return true;
}

bool pci_set_irq_unmask(pci_addr_t addr, bool unmask)
{
    bool msix;
    pci_msi_caps_hdr_t caps;
    int capability = pci_find_msi_msix(addr, msix, caps);

    if (!capability)
        return false;

    if (msix) {
        // Update function mask
        PCI_MSIX_MSG_CTRL_MASK_SET(caps.msg_ctrl, !unmask);

        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));
    } else {
        if (PCI_MSI_MSG_CTRL_VMASK_GET(caps.msg_ctrl) &&
                PCI_MSI_MSG_CTRL_CAP64_GET(caps.msg_ctrl)) {
            // Function supports per-vector mask
            uint32_t irq_mask = -!unmask;
            pci_config_write(addr, capability + sizeof(pci_msi_caps_hdr_t) +
                             offsetof(pci_msi32_pvm_t, irq_mask),
                             &irq_mask, sizeof(irq_mask));
        } else {
            // Hardware does not support masking!
            return false;
        }
    }

    return true;
}

int pci_max_vectors(pci_addr_t addr)
{
    pci_msi_caps_hdr_t caps;
    bool msix;

    int capability = pci_find_msi_msix(addr, msix, caps);

    if (unlikely(!capability))
        return 0;

    int count = 0;

    if (msix) {
        count = PCI_MSIX_MSG_CTRL_TBLSZ_GET(caps.msg_ctrl) + 1;
    } else {
        uint8_t multi_exp = PCI_MSI_MSG_CTRL_MMC_GET(caps.msg_ctrl);
        count = 1 << multi_exp;
    }

    return msix ? count : -count;
}

void pci_set_irq_line(pci_addr_t addr, uint8_t irq_line)
{
    pci_config_write(addr, offsetof(pci_config_hdr_t, irq_line),
                     &irq_line, sizeof(irq_line));
}

void pci_set_irq_pin(pci_addr_t addr, uint8_t irq_pin)
{
    pci_config_write(addr, offsetof(pci_config_hdr_t, irq_pin),
                     &irq_pin, sizeof(irq_pin));
}

static void pci_adj_bits_16(pci_addr_t addr, int offset,
                            uint16_t set, uint16_t clr)
{
    if (set || clr) {
        uint16_t reg;
        uint16_t new_reg;

        pci_config_copy(addr, &reg, offset,
                        sizeof(reg));

        new_reg = (reg | set) & ~clr;

        if (new_reg != reg)
            pci_config_write(addr, offset, &new_reg, sizeof(new_reg));
    }
}

void pci_adj_control_bits(pci_addr_t addr, uint16_t set, uint16_t clr)
{
    pci_adj_bits_16(addr, offsetof(pci_config_hdr_t, command), set, clr);
}

void pci_adj_control_bits(pci_dev_iterator_t const& pci_dev,
                          uint16_t set, uint16_t clr)
{
    pci_adj_control_bits((pci_addr_t)pci_dev, set, clr);
}

void pci_clear_status_bits(pci_addr_t addr, uint16_t bits)
{
    pci_config_write(addr, offsetof(pci_config_hdr_t, status),
                     &bits, sizeof(bits));
}

char const *pci_device_class_text(uint8_t cls)
{
    if (cls < sizeof(pci_device_class_text_lookup))
        return pci_device_class_text_lookup[cls];
    return nullptr;
}

bool pci_init_ecam(size_t ecam_count)
{
    return pci_ecam_list.reserve(ecam_count);
}

void pci_init_ecam_entry(uint64_t base, uint16_t seg,
                         uint8_t st_bus, uint8_t en_bus)
{
    char *mapping = (char*)mmap((void*)base, uint64_t(en_bus - st_bus) << 20,
                         PROT_READ | PROT_WRITE,
                         MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU);
    assert(mapping != MAP_FAILED);
    if (unlikely(!pci_ecam_list.push_back(
                     pci_ecam_t{base, mapping, seg, st_bus, en_bus})))
        panic_oom();
}

void pci_init_ecam_enable()
{
    printdbg("Using PCIe ECAM for configuration space accesses\n");
    pci_accessor = &pci_mmio_accessor;
}

uint64_t pci_config_hdr_t::get_bar(size_t bar) const
{
    uint64_t addr;

    if (is_bar_mmio(bar)) {
        addr = base_addr[bar] & PCI_BAR_MMIO_BA;

        if (is_bar_64bit(bar))
            addr |= uint64_t(base_addr[bar + 1]) << 32;
    } else {
        // Mask off low 2 bits
        addr = base_addr[bar] & -4;
    }

    return addr;
}

pci_bar_size_t pci_config_hdr_t::get_bar_size(
        pci_addr_t const &pci_addr, size_t bar) const
{
    pci_bar_size_t result;

    size_t bar_offset = (char*)(base_addr + bar) - (char*)&vendor;

    uint64_t backup;
    uint32_t backup32;

    // Make backup
    if (is_bar_64bit(bar)) {
        backup = 0;
        pci_config_copy(pci_addr, &backup, bar_offset, sizeof(backup));

    } else {
        backup32 = 0;
        pci_config_copy(pci_addr, &backup32, bar_offset, sizeof(backup32));
    }

    uint64_t readback = 0;

    if (is_bar_64bit(bar)) {
        uint64_t probe_val = PCI_BAR_MMIO_BA |
                (uint64_t(UINT32_MAX) << 32) |
                (backup & ~PCI_BAR_MMIO_BA);

        pci_config_write(pci_addr, bar_offset,
                         &probe_val, sizeof(probe_val));

        pci_config_copy(pci_addr, &readback,
                        bar_offset, sizeof(readback));
    } else if (is_bar_mmio(bar)) {
        uint32_t mmio_ones32 = PCI_BAR_MMIO_BA |
                (backup32 & ~PCI_BAR_MMIO_BA);

        pci_config_write(pci_addr, bar_offset,
                         &mmio_ones32, sizeof(mmio_ones32));

        uint32_t readback32;
        pci_config_copy(pci_addr, &readback32,
                        bar_offset, sizeof(readback32));

        readback = readback32;
    } else {
        uint32_t io_ones = PCI_BAR_IO_BA |
                (backup32 & ~PCI_BAR_IO_BA);

        pci_config_write(pci_addr, bar_offset,
                         &io_ones, sizeof(io_ones));

        uint32_t readback32;
        pci_config_copy(pci_addr, &readback32,
                        bar_offset, sizeof(readback32));

        readback = readback32;
    }

    uint64_t readback_ba = readback &
            (PCI_BAR_MMIO_BA | (uint64_t(UINT32_MAX) << 32));

    int log2_max_addr = bit_msb_set(readback_ba) + 1;
    int log2_size = bit_lsb_set(readback_ba);

    result = { log2_max_addr, log2_size };

    // Restore backup
    if (is_bar_64bit(bar)) {
        pci_config_write(pci_addr, bar_offset, &backup, sizeof(backup));
    } else {
        pci_config_write(pci_addr, bar_offset, &backup32, sizeof(backup32));
    }

    return result;
}

void pci_config_hdr_t::set_mmio_bar(
        pci_addr_t pci_addr, size_t bar, uint64_t addr)
{
    // PCI 2.2 section 6.2.5

    size_t size;

    if (is_bar_mmio(bar) && is_bar_64bit(bar)) {
        size = sizeof(uint64_t);
    } else {
        size = sizeof(uint32_t);
    }

    uint32_t& selected_bar = base_addr[bar];

    size_t bar_offset = (char*)&selected_bar - (char*)&vendor;

    // Write the value
    pci_config_write(pci_addr, bar_offset, &addr, size);

    // Read it back
    pci_config_copy(pci_addr, (char*)&selected_bar, bar_offset, size);
}

int pci_enumerate_begin(
        pci_dev_iterator_t *iter,
        int dev_class, int subclass, int vendor, int device)
{
    pci_dev_iter_init(iter, dev_class, device, vendor, subclass);

    for (size_t i = 0, e = pci_cache.iters.size(); i != e; ++i) {
        auto const& hdr = pci_cache.iters[i];

        if (pci_enumerate_is_match(&hdr, iter)) {
            iter->copy_from(hdr);
            return 1;
        }
    }

    return 0;
}

int pci_enumerate_next(pci_dev_iterator_t *iter)
{
    auto at = ext::find(pci_cache.iters.begin(),
                        pci_cache.iters.end(), *iter);

    if (likely(at != pci_cache.iters.end()))
        ++at;
    else
        assert(!"Nonsense iterator passed to pci_enumerate_next");

    while (at != pci_cache.iters.end()) {
        if (pci_enumerate_is_match(&*at)) {
            iter->copy_from(*at);
            return 1;
        }
    }

    return 0;
}

pci_addr_t::pci_addr_t()
    : addr(0)
{
}

pci_addr_t::pci_addr_t(int seg, int bus, int slot, int func)
    : addr((uint32_t(seg) << 16) | (bus << 8) | (slot << 3) | (func))
{
    assert(seg >= 0);
    assert(seg < 65536);
    assert(bus >= 0);
    assert(bus < 256);
    assert(slot >= 0);
    assert(slot < 32);
    assert(func >= 0);
    assert(func < 8);
}

int pci_addr_t::bus() const
{
    return (addr >> 8) & 0xFF;
}

int pci_addr_t::slot() const
{
    return (addr >> 3) & 0x1F;
}

int pci_addr_t::func() const
{
    return addr & 0x7;
}

bool pci_addr_t::is_legacy() const
{
    return (addr < 65536);
}

uint64_t pci_addr_t::get_addr() const
{
    return addr << 12;
}

pci_dev_iterator_t::operator pci_addr_t() const
{
    return pci_addr_t(segment, bus, slot, func);
}

void pci_dev_iterator_t::reset()
{
    segment = 0;
    bus = 0;
    slot = 0;
    func = 0;
    dev_class = 0;
    subclass = 0;
    vendor = 0;
    device = 0;
    header_type = 0;
    bus_todo_len = 0;
    config = {};
    memset(bus_todo, 0, sizeof(bus_todo));
}

pci_dev_iterator_t &pci_dev_iterator_t::copy_from(
        pci_dev_iterator_t const& rhs)
{
    segment = rhs.segment;
    bus = rhs.bus;
    slot = rhs.slot;
    func = rhs.func;

    config = rhs.config;
    addr = pci_addr_t(segment, bus, slot, func);
    return *this;
}

bool pci_dev_iterator_t::operator==(pci_dev_iterator_t const& rhs) const
{
    return (segment == rhs.segment) &
            (bus == rhs.bus) &
            (slot == rhs.slot) &
            (func == rhs.func);
}

bool pci_config_hdr_t::is_bar_mmio(size_t bar) const
{
    assert(bar < countof(base_addr));
    return PCI_BAR_RTE_GET(base_addr[bar]) == 0;
}

bool pci_config_hdr_t::is_bar_portio(size_t bar) const
{
    assert(bar < countof(base_addr));
    return PCI_BAR_RTE_GET(base_addr[bar]) != 0;
}

bool pci_config_hdr_t::is_bar_prefetchable(size_t bar) const
{
    assert(bar < countof(base_addr));
    return (PCI_BAR_RTE_GET(base_addr[bar]) == 0) &&
            PCI_BAR_MMIO_PF_GET(base_addr[bar]);
}

bool pci_config_hdr_t::is_bar_64bit(size_t bar) const
{
    assert(bar < countof(base_addr));
    return (PCI_BAR_RTE_GET(base_addr[bar]) == 0) &&
            (PCI_BAR_MMIO_TYPE_GET(base_addr[bar]) == PCI_BAR_MMIO_TYPE_64BIT);
}

pci_dev_t::pci_dev_t()
{
}

pci_dev_t::~pci_dev_t()
{

}

pci_dev_iterator_t::pci_dev_iterator_t()
{

}

pci_dev_iterator_t::~pci_dev_iterator_t()
{

}

char const *pci_describe_device_iter(pci_dev_iterator_t const& pci_iter)
{
    return pci_describe_device(pci_iter.config.dev_class,
                               pci_iter.config.subclass,
                               pci_iter.config.prog_if);
}

char const *pci_describe_device(uint8_t cls, uint8_t sc, uint8_t pif)
{
    switch (cls) {
    case PCI_DEV_CLASS_UNCLASSIFIED:
        switch (sc) {
        case PCI_SUBCLASS_UNCLASSIFIED_OLD:
            return "Unclassified/Old";
        case PCI_SUBCLASS_UNCLASSIFIED_VGA:
            return "Unclassified/VGA";
        default:
            return "Unclassified/Unknown";
        }
    case PCI_DEV_CLASS_STORAGE:
        switch (sc) {
        case PCI_SUBCLASS_STORAGE_SCSI:
            return "Storage/SCSI";
        case PCI_SUBCLASS_STORAGE_IDE:
            return "Storage/IDE";
        case PCI_SUBCLASS_STORAGE_FLOPPY:
            return "Storage/Floppy";
        case PCI_SUBCLASS_STORAGE_IPIBUS:
            return "Storage/IPIBus";
        case PCI_SUBCLASS_STORAGE_RAID:
            return "Storage/RAID";
        case PCI_SUBCLASS_STORAGE_ATA:
            return "Storage/ATA";
        case PCI_SUBCLASS_STORAGE_SATA:
            switch (pif) {
            case PCI_PROGIF_STORAGE_SATA_VEND:
                return "Storage/SATA/Vendor Specific";
            case PCI_PROGIF_STORAGE_SATA_AHCI:
                return "Storage/SATA/AHCI";
            case PCI_PROGIF_STORAGE_SATA_SERIAL:
                return "Storage/SATA/Serial";
            default:
                return "Storage/SATA/Unknown";
            }
        case PCI_SUBCLASS_STORAGE_SAS:
            return "Storage/SAS";
        case PCI_SUBCLASS_STORAGE_NVM:
            switch (pif) {
            case PCI_PROGIF_STORAGE_NVM_NVME:
                return "Storage/NVM/NVMe";
            default:
                return "Storage/NVM/Unknown";
            }
        case PCI_SUBCLASS_STORAGE_MASS:
            return "Storage/Mass";
        default:
            return "Storage/Unknown";
        }
    case PCI_DEV_CLASS_NETWORK:
        switch (sc) {
        case PCI_SUBCLASS_NETWORK_ETHERNET:
            return "Network/Ethernet";
        case PCI_SUBCLASS_NETWORK_TOKENRING:
            return "Network/TokenRing";
        case PCI_SUBCLASS_NETWORK_FDDI:
            return "Network/FDDI";
        case PCI_SUBCLASS_NETWORK_ATM:
            return "Network/ATM";
        case PCI_SUBCLASS_NETWORK_ISDN:
            return "Network/IDSN";
        case PCI_SUBCLASS_NETWORK_WFLIP:
            return "Network/WFLIP";
        case PCI_SUBCLASS_NETWORK_PICMGMC:
            return "Network/PICMGMC";
        case PCI_SUBCLASS_NETWORK_OTHER:
            return "Network/Other";
        default:
            return "Network/Unknown";
        }
    case PCI_DEV_CLASS_DISPLAY:
        switch (sc) {
        case PCI_SUBCLASS_DISPLAY_VGA:
            switch (pif) {
            case PCI_PROGIF_DISPLAY_VGA_STD:
                return "Display/VGA/Standard";
            case PCI_PROGIF_DISPLAY_VGA_8514:
                return "Display/VGA/8514";
            default:
                return "Display/VGA/Unknown";
            }
        case PCI_SUBCLASS_DISPLAY_XGA:
            return "Display/XGA";
        case PCI_SUBCLASS_DISPLAY_3D:
            return "Display/3D";
        case PCI_SUBCLASS_DISPLAY_OTHER:
            return "Display/Other";
        default:
            return "Display/Unknown";
        }
    case PCI_DEV_CLASS_MULTIMEDIA:
        switch (sc) {
        case PCI_SUBCLASS_MULTIMEDIA_VIDEO:
            return "Multimedia/Video";
        case PCI_SUBCLASS_MULTIMEDIA_AUDIO:
            return "Multimedia/Audio";
        case PCI_SUBCLASS_MULTIMEDIA_TELEP:
            return "Multimedia/Telephony";
        case PCI_SUBCLASS_MULTIMEDIA_OTHER:
            return "Multimedia/Other";
        default:
            return "Multimedia/Unknown";
        }
    case PCI_DEV_CLASS_MEMORY:
        switch (sc) {
        case PCI_SUBCLASS_MEMORY_RAM:
            return "Memory/RAM";
        case PCI_SUBCLASS_MEMORY_FLASH:
            return "Memory/Flash";
        case PCI_SUBCLASS_MEMORY_OTHER:
            return "Memory/Other";
        default:
            return "Memory/Unknown";
        }
    case PCI_DEV_CLASS_BRIDGE:
        switch (sc) {
        case PCI_SUBCLASS_BRIDGE_HOST:
            return "Bridge/Host";
        case PCI_SUBCLASS_BRIDGE_ISA:
            return "Bridge/ISA";
        case PCI_SUBCLASS_BRIDGE_EISA:
            return "Bridge/EISA";
        case PCI_SUBCLASS_BRIDGE_MCA:
            return "Bridge/MCA";
        case PCI_SUBCLASS_BRIDGE_PCI2PCI:
            switch (pif) {
            case PCI_SUBCLASS_PCI2PCI_NORMAL:
                return "Bridge/PCI2PCI/Normal";
            case PCI_SUBCLASS_PCI2PCI_SUBTRAC:
                return "Bridge/PCI2PCI/Subtractive";
            default:
                return "Bridge/PCI2PCI/Unknown";
            }
        case PCI_SUBCLASS_BRIDGE_PCMCIA:
            return "Bridge/PCMCIA";
        case PCI_SUBCLASS_BRIDGE_NUBUS:
            return "Bridge/NuBus";
        case PCI_SUBCLASS_BRIDGE_CARDBUS:
            return "Bridge/CardBus";
        case PCI_SUBCLASS_BRIDGE_RACEWAY:
            return "Bridge/RaceWay";
        case PCI_SUBCLASS_BRIDGE_SEMITP2P:
            switch (pif) {
            case PCI_PROGIF_BRIDGE_SEMITP2P_P:
                return "Bridge/SEMITP2P/P";
            case PCI_PROGIF_BRIDGE_SEMITP2P_S:
                return "Bridge/SEMITP2P/S";
            default:
                return "Bridge/SEMITP2P/Unknown";
            }
        case PCI_SUBCLASS_BRIDGE_INFINITI:
            return "Bridge/Infiniti";
        case PCI_SUBCLASS_BRIDGE_OTHER:
            return "Bridge/Other";
        default:
            return "Bridge/Unknown";
        }
    case PCI_DEV_CLASS_COMM:
        switch (sc) {
        case PCI_SUBCLASS_COMM_16x50:
            switch (pif) {
            case PCI_PROGIF_COMM_16x50_XT:
                return "Comm/16x50/XT";
            case PCI_PROGIF_COMM_16x50_16450:
                return "Comm/16x50/16450";
            case PCI_PROGIF_COMM_16x50_16550:
                return "Comm/16x50/16550";
            case PCI_PROGIF_COMM_16x50_16650:
                return "Comm/16x50/16650";
            case PCI_PROGIF_COMM_16x50_16750:
                return "Comm/16x50/16750";
            case PCI_PROGIF_COMM_16x50_16850:
                return "Comm/16x50/16850";
            case PCI_PROGIF_COMM_16x50_16960:
                return "Comm/16x50/16950";
            default:
                return "Comm/16x50/Unknown";
            }
        case PCI_SUBCLASS_COMM_PARALLEL:
            // PCI_SUBCLASS_COMM_PARALLEL
            switch (pif) {
            case PCI_PROGIF_COMM_PARALLEL_BASIC:
                return "Comm/Parallel/Basic";
            case PCI_PROGIF_COMM_PARALLEL_BIDIR:
                return "Comm/Parallel/Bidirectional";
            case PCI_PROGIF_COMM_PARALLEL_ECP:
                return "Comm/Parallel/ECP";
            case PCI_PROGIF_COMM_PARALLEL_1284:
                return "Comm/Parallel/1284";
            case PCI_PROGIF_COMM_PARALLEL_1284D:
                return "Comm/Parallel/1284D";
            default:
                return "Comm/Parallel/Unknown";
            }
        case PCI_SUBCLASS_COMM_MULTIPORT:
            return "Comm/Multiport";
        case PCI_SUBCLASS_COMM_MODEM:
            switch (pif) {
            case PCI_PROGIF_COMM_MODEM_GENERIC:
                return "Comm/Modem/Generic";
            case PCI_PROGIF_COMM_MODEM_HAYES_450:
                return "Comm/Modem/Hayes_450";
            case PCI_PROGIF_COMM_MODEM_HAYES_550:
                return "Comm/Modem/Hayes_550";
            case PCI_PROGIF_COMM_MODEM_HAYES_650:
                return "Comm/Modem/Hayes_650";
            case PCI_PROGIF_COMM_MODEM_HAYES_750:
                return "Comm/Modem/Hayes_750";
            default:
                return "Comm/Modem/Unknown";
            }
        case PCI_SUBCLASS_COMM_GPIB:
            return "Comm/GPIB";
        case PCI_SUBCLASS_COMM_SMARTCARD:
            return "Comm/SmartCard";
        case PCI_SUBCLASS_COMM_OTHER:
            return "Comm/Other";
        default:
            return "Comm/Unknown";
        }
    case PCI_DEV_CLASS_SYSTEM:
        switch (sc) {
        case PCI_SUBCLASS_SYSTEM_PIC:
            switch (pif) {
            case PCI_PROGIF_SYSTEM_PIC_8259:
                return "System/PIC/8259";
            case PCI_PROGIF_SYSTEM_PIC_ISA:
                return "System/PIC/ISA";
            case PCI_PROGIF_SYSTEM_PIC_EISA:
                return "System/PIC/EISA";
            case PCI_PROGIF_SYSTEM_PIC_IOAPIC:
                return "System/PIC/IOAPIC";
            case PCI_PROGIF_SYSTEM_PIC_IOXAPIC:
                return "System/PIC/IOXAPIC";
            default:
                return "System/PIC/Unknown";
            }
        case PCI_SUBCLASS_SYSTEM_DMA:
            switch (pif) {
            case PCI_PROGIF_SYSTEM_DMA_8237:
                return "System/DMA/8237";
            case PCI_PROGIF_SYSTEM_DMA_ISA:
                return "System/DMA/ISA";
            case PCI_PROGIF_SYSTEM_DMA_EISA:
                return "System/DMA/EISA";
            default:
                return "System/DMA/Unknown";
            }
        case PCI_SUBCLASS_SYSTEM_TIMER:
            switch (pif) {
            case PCI_PROGIF_SYSTEM_TIMER_8254:
                return "System/Timer/8254";
            case PCI_PROGIF_SYSTEM_TIMER_ISA:
                return "System/Timer/ISA";
            case PCI_PROGIF_SYSTEM_TIMER_EISA:
                return "System/Timer/EISA";
            default:
                return "System/Timer/Unknown";
            }
        case PCI_SUBCLASS_SYSTEM_RTC:
            switch (pif) {
            case PCI_PROGIF_SYSTEM_RTC_GENERIC:
                return "System/RTC/Generic";
            case PCI_PROGIF_SYSTEM_RTC_ISA:
                return "System/RTC/ISA";
            default:
                return "System/RTC/Unknown";
            }
        case PCI_SUBCLASS_SYSTEM_HOTPLUG:
            return "System/Hotplug";
        case PCI_SUBCLASS_SYSTEM_SDHOST:
            return "System/SDHost";
        case PCI_SUBCLASS_SYSTEM_OTHER:
            return "System/Other";
        default:
            return "System/Unknown";
        }
    case PCI_DEV_CLASS_INPUT:
        switch (sc) {
        case PCI_SUBCLASS_INPUT_KEYBOARD:
            return "Input/Keyboard";
        case PCI_SUBCLASS_INPUT_DIGIPEN:
            return "Input/DigiPen";
        case PCI_SUBCLASS_INPUT_MOUSE:
            return "Input/Mouse";
        case PCI_SUBCLASS_INPUT_SCANNER:
            return "Input/Scanner";
        case PCI_SUBCLASS_INPUT_GAME:
            switch (pif) {
            case PCI_PROGIF_INPUT_GAME_GENERIC:
                return "Input/Game/Generic";
            case PCI_PROGIF_INPUT_GAME_STD:
                return "Input/Game/Standard";
            default:
                return "Input/Game/Unknown";
            }
        case PCI_SUBCLASS_INPUT_OTHER:
            return "Input/Other";
        default:
            return "Input/Unknown";
        }
    case PCI_DEV_CLASS_DOCKING:
        switch (sc) {
        case PCI_SUBCLASS_DOCKING_GENERIC:
            return "Docking/Generic";
        case PCI_SUBCLASS_DOCKING_OTHER:
            return "Docking/Other";
        default:
            return "Docking/Unknown";
        }
    case PCI_DEV_CLASS_PROCESSOR:
        switch (sc) {
        case PCI_SUBCLASS_PROCESSOR_386:
            return "Processor/386";
        case PCI_SUBCLASS_PROCESSOR_486:
            return "Processor/486";
        case PCI_SUBCLASS_PROCESSOR_PENTIUM:
            return "Processor/Pentium";
        case PCI_SUBCLASS_PROCESSOR_ALPHA:
            return "Processor/Alpha";
        case PCI_SUBCLASS_PROCESSOR_PPC:
            return "Processor/PPC";
        case PCI_SUBCLASS_PROCESSOR_MIPS:
            return "Processor/MIPS";
        case PCI_SUBCLASS_PROCESSOR_COPROC:
            return "Processor/Coprocessor";
        default:
            return "Processor/Unknown";
        }
    case PCI_DEV_CLASS_SERIAL:
        switch (sc) {
        case PCI_SUBCLASS_SERIAL_IEEE1394:
            switch (pif) {
            case PCI_PROGIF_SERIAL_IEEE1394_FW:
                return "Serial/IEEE1394/FW";
            default:
                return "Serial/IEEE1394/Unknown";
            }
        case PCI_SUBCLASS_SERIAL_ACCESS:
        case PCI_SUBCLASS_SERIAL_SSA:
        case PCI_SUBCLASS_SERIAL_USB:
            switch (pif) {
            case PCI_PROGIF_SERIAL_USB_UHCI:
                return "Serial/USB/UHCI";
            case PCI_PROGIF_SERIAL_USB_OHCI:
                return "Serial/USB/OHCI";
            case PCI_PROGIF_SERIAL_USB_EHCI:
                return "Serial/USB/EHCI";
            case PCI_PROGIF_SERIAL_USB_XHCI:
                return "Serial/USB/XHCI";
            case PCI_PROGIF_SERIAL_USB_UNSPEC:
                return "Serial/USB/Unspecified";
            case PCI_PROGIF_SERIAL_USB_USBDEV:
                return "Serial/USB/USBDev";
            default:
                return "Serial/USB/Unknown";
            }
        case PCI_SUBCLASS_SERIAL_FIBRECHAN:
            return "Serial/FibreChannel";
        case PCI_SUBCLASS_SERIAL_SMBUS:
            return "Serial/SMBus";
        case PCI_SUBCLASS_SERIAL_INFINIBAND:
            return "Serial/InfiniBand";
        case PCI_SUBCLASS_SERIAL_IPMI:
            switch (pif) {
            case PCI_PROGIF_SERIAL_IPMI_SMIC:
                return "Serial/IPMI/SMIC";
            case PCI_PROGIF_SERIAL_IPMI_KEYBD:
                return "Serial/IPMI/Keyboard";
            case PCI_PROGIF_SERIAL_IPMI_BLOCK:
                return "Serial/IPMI/Block";
            default:
                return "Serial/IPMI/Unknown";
            }
        case PCI_SUBCLASS_SERIAL_SERCOS:
            return "Serial/SerCos";
        case PCI_SUBCLASS_SERIAL_CANBUS:
            return "Serial/CanBus";
        default:
            return "Serial/Unknown";
        }
    case PCI_DEV_CLASS_WIRELESS:
        switch (sc) {
        case PCI_SUBCLASS_WIRELESS_IRDA:
            return "Wireless/IRDA";
        case PCI_SUBCLASS_WIRELESS_IR:
            return "Wireless/IR";
        case PCI_SUBCLASS_WIRELESS_RF:
            return "Wireless/RF";
        case PCI_SUBCLASS_WIRELESS_BLUETOOTH:
            return "Wireless/BlueTooth";
        case PCI_SUBCLASS_WIRELESS_BROADBAND:
            return "Wireless/Broadband";
        case PCI_SUBCLASS_WIRELESS_ETH5GHz:
            return "Wireless/Ethernet5GHz";
        case PCI_SUBCLASS_WIRELESS_ETH2GHz:
            return "Wireless/Ethernet2.4GHz";
        case PCI_SUBCLASS_WIRELESS_OTHER:
            return "Wireless/Other";
        default:
            return "Wireless/Unknown";
        }
    case PCI_DEV_CLASS_INTELLIGENT:
        switch (sc) {
        case PCI_SUBCLASS_INTELLIGENT_IO:
            switch (pif) {
            case PCI_PROGIF_INTELLIGENT_IO_I2O:
                return "Intelligent/IO/I2O";
            case PCI_PROGIF_INTELLIGENT_IO_FIFO:
                return "Intelligent/IO/FIFO";
            default:
                return "Intelligent/IO/Unknown";
            }
        default:
            return "Intelligent/Unknown";
        }
    case PCI_DEV_CLASS_SATELLITE:
        switch (sc) {
        case PCI_SUBCLASS_SATELLITE_TV:
            return "Satellite/TV";
        case PCI_SUBCLASS_SATELLITE_AUDIO:
            return "Satellite/Audio";
        case PCI_SUBCLASS_SATELLITE_VOICE:
            return "Satellite/Voice";
        case PCI_SUBCLASS_SATELLITE_DATA:
            return "Satellite/Data";
        default:
            return "Satellite/Unknown";
        }
    case PCI_DEV_CLASS_ENCRYPTION:
        switch (sc) {
        case PCI_SUBCLASS_ENCRYPTION_NET:
            return "Encryption/Net";
        case PCI_SUBCLASS_ENCRYPTION_ENTAIN:
            return "Encryption/Entain";
        case PCI_SUBCLASS_ENCRYPTION_OTHER:
            return "Encryption/Other";
        default:
            return "Encryption/Unknown";
        }
    case PCI_DEV_CLASS_DSP:
        switch (sc) {
        case PCI_SUBCLASS_DSP_DPIO:
            return "DSP/DPIO";
        case PCI_SUBCLASS_DSP_PERFCNT:
            return "DSP/PerfCount";
        case PCI_SUBCLASS_DSP_COMMSYNC:
            return "DSP/CommSync";
        case PCI_SUBCLASS_DSP_MGMTCARD:
            return "DSP/ManagementCard";
        case PCI_SUBCLASS_DSP_OTHER:
            return "DSP/Other";
        default:
            return "DSP/Unknown";
        }
    case PCI_DEV_CLASS_ACCELERATOR:
        switch (sc) {
        default:
            return "Accelerator/Unknown";
        }
    case PCI_DEV_CLASS_INSTRUMENTATION:
        switch (sc) {
        default:
            return "Instrumentation/Unknown";
        }
    case PCI_DEV_CLASS_COPROCESSOR:
        switch (sc) {
        default:
            return "Coprocessor/Unknown";
        }
    case PCI_DEV_CLASS_UNASSIGNED:
        switch (sc) {
        default:
            return "Unassigned/Unknown";
        }
    default:
        return "Unknown";
    }
}

void *pci_bar_accessor_t::map(uint64_t addr, size_t length)
{
    void *mem = mmap((void*)addr, length, PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU);

    return mem;
}

pci_bar_accessor_t::pci_bar_accessor_t(
        const pci_dev_iterator_t &pci_iter, unsigned bar, size_t length)
{
    uint64_t addr = pci_iter.config.get_bar(bar);

    pci_bar_size_t bar_size = pci_iter.config.get_bar_size(pci_iter.addr, bar);

    printdbg("pci: bar %d decodes %d bits, size is %zu\n",
             bar, bar_size.log2_max_addr, size_t(1) << bar_size.log2_size);

    if (pci_iter.config.is_bar_mmio(bar)) {
        void *mem = map(addr, length);

        if (unlikely(mem == MAP_FAILED))
            return;

        mmio_base = (uint64_t)mem;
        mmio_size = length;
    } else {
        port_base = addr;
    }
}

pci_bar_accessor_t::~pci_bar_accessor_t()
{
    if (mmio_size)
        munmap((void*)mmio_base, mmio_size);

    mmio_base = 0;
    mmio_size = 0;
}

void pci_bar_accessor_t::wr_8(pci_bar_register_t reg, uint8_t val)
{
    assert(reg.size == sizeof(val));

    if (likely(mmio_base))
        return mm_wr(*(uint8_t*)(mmio_base + reg.offset), val);

    return outb(port_base + reg.offset, val);
}

void pci_bar_accessor_t::wr_16(pci_bar_register_t reg, uint16_t val)
{
    assert(reg.size == sizeof(val));

    if (likely(mmio_base))
        return mm_wr(*(uint16_t*)(mmio_base + reg.offset), val);

    return outw(port_base + reg.offset, val);
}

void pci_bar_accessor_t::wr_32(pci_bar_register_t reg, uint32_t val)
{
    assert(reg.size == sizeof(val));

    if (likely(mmio_base))
        return mm_wr(*(uint32_t*)(mmio_base + reg.offset), val);

    return outd(port_base + reg.offset, val);
}

void pci_bar_accessor_t::wr_64(pci_bar_register_t reg, uint64_t val)
{
    assert(reg.size == sizeof(val));

    if (likely(mmio_base))
        return mm_wr(*(uint64_t*)(mmio_base + reg.offset), val);

    outd(port_base + reg.offset, val);
    return outd(port_base + reg.offset + sizeof(uint32_t), val >> 32);
}

uint8_t pci_bar_accessor_t::rd_8(pci_bar_register_t reg)
{
    assert(reg.size == sizeof(uint8_t));

    if (likely(mmio_base))
        return mm_rd(*(uint8_t*)(mmio_base + reg.offset));

    return inb(port_base + reg.offset);
}

uint16_t pci_bar_accessor_t::rd_16(pci_bar_register_t reg)
{
    assert(reg.size == sizeof(uint16_t));

    if (likely(mmio_base))
        return mm_rd(*(uint16_t*)(mmio_base + reg.offset));

    return inw(port_base + reg.offset);
}

uint32_t pci_bar_accessor_t::rd_32(pci_bar_register_t reg)
{
    assert(reg.size == sizeof(uint32_t));

    if (likely(mmio_base))
        return mm_rd(*(uint32_t*)(mmio_base + reg.offset));

    return ind(port_base + reg.offset);
}

uint64_t pci_bar_accessor_t::rd_64(pci_bar_register_t reg)
{
    assert(reg.size == sizeof(uint64_t));

    if (likely(mmio_base))
        return mm_rd(*(uint64_t*)(mmio_base + reg.offset));

    uint32_t lo = ind(port_base + reg.offset);
    uint32_t hi = ind(port_base + reg.offset + sizeof(uint32_t));

    return (uint64_t(hi) << 32) | lo;
}
