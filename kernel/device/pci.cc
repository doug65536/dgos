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

#define PCI_DEBUG   0
#if PCI_DEBUG
#define PCI_TRACE(...) printk("pci: " __VA_ARGS__)
#else
#define PCI_TRACE(...) ((void)0)
#endif

struct pci_ecam_t;

class pci_config_rw {
public:
    // Read up to 32 bits from PCI config space.
    // Value must be entirely within a single 32 bit dword.
    virtual uint32_t read(pci_addr_t addr, size_t offset, size_t size) = 0;

    // Write an arbitrarily sized and aligned block of values
    virtual bool write(pci_addr_t addr, size_t offset,
                           void const *values, size_t size) = 0;

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
               void const *values, size_t size) override final;
    bool copy(pci_addr_t addr, void *dest,
              size_t offset, size_t size) override final;
};

// Modern PCIe ECAM MMIO PCI configuration space accessor
class pci_config_mmio : public pci_config_rw {
    inline pci_ecam_t *find_ecam(int bus);
public:
    // pci_config_rw interface
    uint32_t read(pci_addr_t addr, size_t offset, size_t size) override final;
    bool write(pci_addr_t addr, size_t offset,
               void const *values, size_t size) override final;
    bool copy(pci_addr_t addr, void *dest,
              size_t offset, size_t size) override final;
};

//
// ECAM

struct pci_ecam_t {
    uint64_t base;
    char *mapping;
    uint16_t segment;
    uint8_t st_bus;
    uint8_t en_bus;
};

static std::vector<pci_ecam_t> pci_ecam_list;

static pci_config_pio pci_pio_accessor;
static pci_config_mmio pci_mmio_accessor;

using pci_lock_type = ext::mcslock;
using pci_scoped_lock = std::unique_lock<pci_lock_type>;
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

static std::vector<std::pair<pci_addr_t, pci_msix_mappings>> pcix_tables;

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
    assert(offset < 256);
    assert((offset & -4) == ((offset + size - 1) & -4));

    uint32_t pci_address = (1 << 31) | addr.get_addr() | (offset & -4);

    pci_scoped_lock lock(pci_lock);

    outd(PCI_ADDR, pci_address);
    uint32_t data = ind(PCI_DATA);

    lock.unlock();

    data >>= (offset & 3) << 3;

    if (size != sizeof(uint32_t))
        data &= ~((uint32_t)-1 << (size << 3));

    return data;
}

bool pci_config_pio::write(pci_addr_t addr, size_t offset,
                           void const *values, size_t size)
{
    // Validate
    if (!(offset < 256 && size <= 256 && offset + size <= 256))
        return false;

    // Pointer to input data
    char *p = (char*)values;

    uint32_t pci_address = (1 << 31) | addr.get_addr();

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

    for (size_t i = 0; i < size; i += sizeof(uint32_t)) {
        value = pci_config_read(addr, offset + i, sizeof(value));

        if (i + sizeof(value) <= size)
            memcpy(out + i, &value, sizeof(value));
        else
            memcpy(out + i, &value, size - i);
    }

    return true;
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

    uint64_t ecam_offset = ((bus - ent->st_bus) << 20) + (addr.slot() << 15) +
            (addr.func() << 12) + unsigned(offset);

    if (likely(size == sizeof(uint32_t)))
        return mm_rd(*(uint32_t*)(ent->mapping + ecam_offset));

    if (likely(size == sizeof(uint16_t)))
        return mm_rd(*(uint16_t*)(ent->mapping + ecam_offset));

    if (likely(size == sizeof(uint8_t)))
        return mm_rd(*(uint8_t*)(ent->mapping + ecam_offset));

    panic("Nonsense operand size");

//    ecam_offset &= -4;

//    uint32_t data = mm_rd(*(uint32_t*)(ent->mapping + ecam_offset));

//    data >>= (offset & 3) << 3;

//    if (size != sizeof(uint32_t))
//        data &= ~((uint32_t)-1 << (size << 3));

//    return data;
}

bool pci_config_mmio::write(pci_addr_t addr, size_t offset,
                            void const *values, size_t size)
{
    int bus = addr.bus();

    pci_ecam_t *ent = find_ecam(bus);

    if (unlikely(!ent))
        return false;

    uint64_t ecam_offset = ((bus - ent->st_bus) << 20) + (addr.slot() << 15) +
            (addr.func() << 12) + unsigned(offset);

    switch (size) {
    case 1:
        mm_wr(*(uint8_t volatile *)(ent->mapping + ecam_offset),
              *(uint8_t const*)values);
        return true;

    case 2:
        mm_wr(*(uint16_t volatile*)(ent->mapping + ecam_offset),
              *(uint16_t const*)values);
        return true;

    case 4:
        mm_wr(*(uint32_t volatile *)(ent->mapping + ecam_offset),
              *(uint32_t const*)values);
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

    uint64_t ecam_offset = ((bus - ent->st_bus) << 20) + (addr.slot() << 15) +
            (addr.func() << 12) + unsigned(offset);

    switch (size) {
    case sizeof(uint8_t):
        *(uint8_t*)dest = mm_rd(*(uint8_t volatile const *)
                                (ent->mapping + ecam_offset));
        return true;
    case sizeof(uint16_t):
        *(uint16_t*)dest = mm_rd(*(uint16_t volatile const *)
                                (ent->mapping + ecam_offset));
        return true;
    case sizeof(uint32_t):
        *(uint32_t*)dest = mm_rd(*(uint32_t volatile const *)
                                (ent->mapping + ecam_offset));
        return true;
    }

    mm_copy_rd(dest, ent->mapping + ecam_offset, size);
    return true;
}

EXPORT uint32_t pci_config_read(pci_addr_t addr, int offset, int size)
{
    return pci_accessor->read(addr, offset, size);
}

EXPORT bool pci_config_write(pci_addr_t addr, size_t offset,
                             void *values, size_t size)
{
    return pci_accessor->write(addr, offset, values, size);
}

EXPORT void pci_config_copy(pci_addr_t addr, void *dest, int ofs, size_t size)
{
    pci_accessor->copy(addr, dest, ofs, size);
}

static void pci_enumerate_read(pci_addr_t addr, pci_config_hdr_t *config)
{
    pci_config_copy(addr, config, 0, sizeof(pci_config_hdr_t));
}

static int pci_enumerate_is_match(pci_dev_iterator_t *iter,
                                  pci_dev_iterator_t *blueprint = nullptr)
{
    if (blueprint == nullptr)
        blueprint = iter;

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
        ++iter->func;

        if (((iter->header_type & 0x80) && iter->func < 8) ||
                iter->func < 1) {
            // Might be a device here
            pci_enumerate_read(*iter, &iter->config);

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

    printk("Autoconfiguring PCI BARs\n");

    do {
        printk("Probing %d:%d:%d\n",
                  pci_iter.bus, pci_iter.slot, pci_iter.func);

        pci_cache.iters.push_back(pci_iter);

        bool any_mmio = false;
        bool any_io = false;
        bool is_64 = false;

        for (int bar = 0; bar < 6; bar += is_64 ? 2 : 1) {
            is_64 = pci_iter.config.is_bar_64bit(bar);

            // Ignore I/O space BARs
            if (!pci_iter.config.is_bar_mmio(bar)) {
                any_io = true;
                continue;
            }

            // Save original base
            uint64_t orig = pci_iter.config.get_bar(bar);

            // Skip already assigned
            if (orig & -16)
                continue;

            // Write all ones
            pci_iter.config.set_mmio_bar(pci_iter, bar, -1);

            // Get readback value
            uint64_t readback = pci_iter.config.get_bar(bar);

            // Mask off information bits 3:0
            readback &= -16;

            // Ignore BAR if it resulted in zero
            if (!readback)
                continue;

            any_mmio = true;

            // Get size
            uint8_t log2_size = bit_lsb_set(readback & -16);
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

        pci_adj_control_bits(pci_iter, (any_mmio ? PCI_CMD_MSE : 0) |
                             (any_io ? PCI_CMD_IOSE : 0) | PCI_CMD_BME,
                             (!any_mmio ? PCI_CMD_MSE : 0) |
                             (!any_io ? PCI_CMD_IOSE : 0));
    } while (pci_enumerate_next_direct(&pci_iter));

    pci_cache.updated_at = time_ns();

    return 0;
}

static uint64_t pci_set_bar(pci_addr_t addr, int bir)
{
    uint64_t bar;
    uint32_t bar_ofs = offsetof(pci_config_hdr_t, base_addr) +
            bir * sizeof(uint32_t);

    pci_config_copy(addr, &bar, bar_ofs, sizeof(bar));

    bool use32 = (PCI_BAR_TYPE_GET(bar) == 0);

    if (use32)
        bar &= 0xFFFFFFFF;

    int bar_width = use32 ? sizeof(uint32_t) : sizeof(uint64_t);

    // Autodetect address space needed by writing all bits one in BAR
    uint64_t new_bar = bar;
    PCI_BAR_BA_SET(new_bar, PCI_BAR_BA_MASK);
    pci_config_write(addr, bar_ofs, &new_bar, bar_width);

    // Read back BAR, size needed is indicated by number of LSB zero bits
    pci_config_copy(addr, &new_bar, bar_ofs, bar_width);

    uint8_t log2_sz = bit_lsb_set_64(new_bar & PCI_BAR_BA_MASK);

    uint32_t alloc_sz = 1U << log2_sz;

    // Allocate twice as much to align
    uint32_t bar_addr = mm_alloc_hole(alloc_sz << 1);

    // Naturally align
    bar_addr = (bar_addr + alloc_sz) & -(alloc_sz);

    // Update base address
    PCI_BAR_BA_SET(new_bar, bar_addr);

    // Write back BAR
    pci_config_write(addr, bar_ofs, &new_bar, bar_width);

    return bar_addr;
}

static int pci_enum_capabilities_match(
        uint8_t id, int ofs, uintptr_t context)
{
    if (id == context)
        return ofs;
    return 0;
}

EXPORT int pci_enum_capabilities(int start, pci_addr_t addr,
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
        uint16_t type_next = pci_config_read(addr, ofs, 2);
        uint8_t type = type_next & 0xFF;
        uint16_t next = (type_next >> 8) & 0xFF;

        PCI_TRACE("cap: ofs=%#x, bus=%d, func=%d, type=%#x, next=%#x\n",
                  ofs, addr.bus(), addr.func(), type, next);

        int result = callback(type, ofs, context);
        if (result != 0)
            return result;

        ofs = next;
    }

    return 0;
}

EXPORT int pci_find_capability(pci_addr_t addr, int capability_id, int start)
{
    return pci_enum_capabilities(start, addr, pci_enum_capabilities_match,
                                 capability_id);
}

EXPORT bool pci_try_msi_irq(pci_dev_iterator_t const& pci_dev,
                            pci_irq_range_t *irq_range,
                            int cpu, bool distribute, int req_count,
                            intr_handler_t handler,
                            char const *name, int const *target_cpus,
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

        pci_set_irq_pin(pci_dev.addr, pci_dev.config.irq_pin);
        pci_set_irq_line(pci_dev.addr, pci_dev.config.irq_line);

        irq_hook(pci_dev.config.irq_line, handler, name);
        irq_setmask(pci_dev.config.irq_line, true);
    }

    return use_msi;
}

static int pci_find_msi_msix(pci_addr_t addr,
                             bool& msix, pci_msi_caps_hdr_t& caps)
{
    // Look for the MSI-X extended capability
    int capability = pci_find_capability(addr, PCICAP_MSIX);

    if (capability) {
        msix = true;
        PCI_TRACE("Found MSI-X capability for PCI %u:%u:%u\n",
                  addr.bus(), addr.slot(), addr.func());
    } else {
        // Fall back to MSI
        msix = false;
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
        PCI_TRACE("No MSI/MSI-X capability for PCI %u:%u:%u\n",
                  addr.bus(), addr.slot(), addr.func());
        return 0;
    }

    return capability;
}

// Infer the number of vectors needed from the highest vector offset
EXPORT int pci_vector_count_from_offsets(int const *vector_offsets, int count)
{
    return std::accumulate(vector_offsets, vector_offsets + count, 0,
                      [&](int highest, int const& item) {
        return std::max(highest, item);
    }) + 1;
}

// Returns with the function masked if possible
// Use pci_set_irq_mask to unmask it when appropriate
EXPORT bool pci_set_msi_irq(pci_addr_t addr, pci_irq_range_t *irq_range,
                            int cpu, bool distribute, size_t req_count,
                            intr_handler_t handler, char const *name,
                            int const *target_cpus, int const *vector_offsets)
{
    bool msix;
    pci_msi_caps_hdr_t caps;
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

        // Read the BAR indicated by the table BIR
        pci_config_copy(addr, &tbl_base,
                        offsetof(pci_config_hdr_t, base_addr) +
                        sizeof(uint32_t) * tbl_bir, sizeof(tbl_base));

        // Read the BAR indicated by the PBA BIR
        pci_config_copy(addr, &pba_base,
                        offsetof(pci_config_hdr_t, base_addr) +
                        sizeof(uint32_t) * pba_bir, sizeof(pba_base));

        // Mask off upper 32 bits if BAR is 32 bit
        if (PCI_BAR_TYPE_GET(tbl_base) == 0)
            tbl_base &= 0xFFFFFFFF;
        if (PCI_BAR_TYPE_GET(pba_base) == 0)
            pba_base &= 0xFFFFFFFF;

        // Handle uninitialized table BAR
        if (unlikely(PCI_BAR_BA_GET(tbl_base) == 0)) {
            tbl_base = pci_set_bar(addr, tbl_bir);

            if (tbl_bir == pba_bir)
                pba_base = tbl_base;
        }

        // Handle uninitialized PBA BAR
        if (unlikely((pba_base & -8) == 0 && tbl_bir != pba_bir))
            pba_base = pci_set_bar(addr, pba_bir);

        tbl_base += tbl_ofs;
        pba_base += pba_ofs;

        pci_msix64_t volatile *tbl = (pci_msix64_t*)
                mmap((void*)(tbl_base & -16), tbl_sz, PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

        uint32_t volatile *pba = (uint32_t*)
                mmap((void*)(pba_base & -16), pba_sz, PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

        pcix_tables.emplace_back(addr, pci_msix_mappings{
                                     tbl, pba, tbl_sz, pba_sz });

        size_t tbl_cnt = std::min(req_count, table_count);

        std::vector<msi_irq_mem_t> msi_writes(tbl_cnt);

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

EXPORT bool pci_set_irq_unmask(pci_addr_t addr, bool unmask)
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

EXPORT int pci_max_vectors(pci_addr_t addr)
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

EXPORT void pci_set_irq_line(pci_addr_t addr, uint8_t irq_line)
{
    pci_config_write(addr, offsetof(pci_config_hdr_t, irq_line),
                     &irq_line, sizeof(irq_line));
}

EXPORT void pci_set_irq_pin(pci_addr_t addr, uint8_t irq_pin)
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

EXPORT void pci_adj_control_bits(pci_addr_t addr, uint16_t set, uint16_t clr)
{
    pci_adj_bits_16(addr, offsetof(pci_config_hdr_t, command), set, clr);
}

EXPORT void pci_adj_control_bits(pci_dev_iterator_t const& pci_dev,
                          uint16_t set, uint16_t clr)
{
    pci_adj_control_bits((pci_addr_t)pci_dev, set, clr);
}

EXPORT void pci_clear_status_bits(pci_addr_t addr, uint16_t bits)
{
    pci_config_write(addr, offsetof(pci_config_hdr_t, status),
                     &bits, sizeof(bits));
}

EXPORT char const *pci_device_class_text(uint8_t cls)
{
    if (cls < sizeof(pci_device_class_text_lookup))
        return pci_device_class_text_lookup[cls];
    return nullptr;
}

void pci_init_ecam(size_t ecam_count)
{
    pci_ecam_list.reserve(ecam_count);
}

void pci_init_ecam_entry(uint64_t base, uint16_t seg,
                         uint8_t st_bus, uint8_t en_bus)
{
    char *mapping = (char*)mmap((void*)base, uint64_t(en_bus - st_bus) << 20,
                         PROT_READ | PROT_WRITE,
                         MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);
    assert(mapping != MAP_FAILED);
    pci_ecam_list.push_back(pci_ecam_t{base, mapping, seg, st_bus, en_bus});
}

void pci_init_ecam_enable()
{
    printdbg("Using PCIe ECAM for configuration space accesses\n");
    pci_accessor = &pci_mmio_accessor;
}

EXPORT uint64_t pci_config_hdr_t::get_bar(ptrdiff_t bar) const
{
    uint64_t addr;

    if (is_bar_mmio(bar)) {
        addr = base_addr[bar] & -16;

        if (is_bar_64bit(bar))
            addr |= uint64_t(base_addr[bar + 1]) << 32;
    } else {
        addr = base_addr[bar] & -4;
    }

    return addr;
}

EXPORT void pci_config_hdr_t::set_mmio_bar(
        pci_addr_t pci_addr, ptrdiff_t bar, uint64_t addr)
{
    // PCI 2.2 section 6.2.5

    size_t size;

    if (is_bar_mmio(bar) && is_bar_64bit(bar)) {
        size = sizeof(uint64_t);
    } else {
        size = sizeof(uint32_t);
    }

    // Write the value
    pci_config_write(pci_addr, (char*)&base_addr[bar] - (char*)this,
                     &addr, size);

    // Read it back
    pci_config_copy(pci_addr, (char*)&base_addr[bar],
                    (char*)&base_addr[bar] - (char*)this, size);
}

EXPORT int pci_enumerate_begin(
        pci_dev_iterator_t *iter,
        int dev_class, int subclass, int vendor, int device)
{
    pci_dev_iter_init(iter, dev_class, device, vendor, subclass);

    for (size_t i = 0, e = pci_cache.iters.size(); i != e; ++i) {
        auto& hdr = pci_cache.iters[i];

        if (pci_enumerate_is_match(&hdr, iter)) {
            iter->copy_from(hdr);
            return 1;
        }
    }

    return 0;
}

EXPORT int pci_enumerate_next(pci_dev_iterator_t *iter)
{
    auto at = std::find(pci_cache.iters.begin(),
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

EXPORT pci_addr_t::pci_addr_t()
    : addr(0)
{
}

EXPORT pci_addr_t::pci_addr_t(int seg, int bus, int slot, int func)
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

EXPORT int pci_addr_t::bus() const
{
    return (addr >> 8) & 0xFF;
}

EXPORT int pci_addr_t::slot() const
{
    return (addr >> 3) & 0x1F;
}

EXPORT int pci_addr_t::func() const
{
    return addr & 0x7;
}

EXPORT bool pci_addr_t::is_legacy() const
{
    return (addr < 65536);
}

EXPORT uint64_t pci_addr_t::get_addr() const
{
    return addr << 12;
}

EXPORT pci_dev_iterator_t::operator pci_addr_t() const
{
    return pci_addr_t(segment, bus, slot, func);
}

EXPORT void pci_dev_iterator_t::reset()
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
    memset(bus_todo, 0, sizeof(bus_todo));
}

EXPORT pci_dev_iterator_t &pci_dev_iterator_t::copy_from(
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

EXPORT bool pci_dev_iterator_t::operator==(pci_dev_iterator_t const& rhs) const
{
    return (segment == rhs.segment) &
            (bus == rhs.bus) &
            (slot == rhs.slot) &
            (func == rhs.func);
}

EXPORT bool pci_config_hdr_t::is_bar_mmio(ptrdiff_t bar) const
{
    return (base_addr[bar] & 1) == 0;
}

EXPORT bool pci_config_hdr_t::is_bar_portio(ptrdiff_t bar) const
{
    return base_addr[bar] & 1;
}

EXPORT bool pci_config_hdr_t::is_bar_prefetchable(ptrdiff_t bar) const
{
    return base_addr[bar] & 8;
}

EXPORT bool pci_config_hdr_t::is_bar_64bit(ptrdiff_t bar) const
{
    return (base_addr[bar] & 7) == 4;
}

EXPORT pci_dev_t::pci_dev_t()
{
}

EXPORT pci_dev_t::~pci_dev_t()
{

}

EXPORT pci_dev_iterator_t::pci_dev_iterator_t()
{

}

EXPORT pci_dev_iterator_t::~pci_dev_iterator_t()
{

}
