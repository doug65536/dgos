#include "pci.h"
#include "cpu/ioport.h"
#include "cpu/spinlock.h"
#include "printk.h"
#include "string.h"
//#include "irq.h"
#include "cpu/apic.h"
#include "vector.h"

#define PCI_DEBUG   0
#if PCI_DEBUG
#define PCI_DEBUGMSG(...) printk(__VA_ARGS__)
#else
#define PCI_DEBUGMSG(...) ((void)0)
#endif

static spinlock_t pci_spinlock;

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
} __packed;

struct pci_msi64_t {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint16_t data;
} __packed;

struct pci_msix64_t {
    uint64_t addr;
    uint32_t data;
    uint32_t ctrl;
};

static vector<pair<pci_addr_t, pci_msix64_t volatile *>> pcix_tables;

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

uint32_t pci_config_read(pci_addr_t addr, int offset, int size)
{
    uint32_t pci_address = (1 << 31) | addr.addr | (offset & -4);

    spinlock_lock_noirq(&pci_spinlock);

    outd(PCI_ADDR, pci_address);
    uint32_t data = ind(PCI_DATA);

    spinlock_unlock_noirq(&pci_spinlock);

    data >>= (offset & 3) << 3;

    if (size != sizeof(uint32_t))
        data &= ~((uint32_t)-1 << (size << 3));

    return data;
}

int pci_config_write(pci_addr_t addr, size_t offset, void *values, size_t size)
{
    // Validate
    if (!(offset < 256 && size <= 256 && offset + size <= 256))
        return 0;

    // Pointer to input data
    char *p = (char*)values;

    uint32_t pci_address = (1 << 31) | addr.addr;

    spinlock_lock_noirq(&pci_spinlock);

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

    spinlock_unlock_noirq(&pci_spinlock);

    return 1;
}

void pci_config_copy(pci_addr_t addr, void *dest, int ofs, size_t size)
{
    uint32_t value;
    char *out = (char*)dest;

    for (size_t i = 0; i < size; i += sizeof(uint32_t)) {
        value = pci_config_read(addr, ofs + i, sizeof(value));

        if (i + sizeof(value) <= size)
            memcpy(out + i, &value, sizeof(value));
        else
            memcpy(out + i, &value, size - i);
    }
}

static void pci_enumerate_read(pci_addr_t addr, pci_config_hdr_t *config)
{
    pci_config_copy(addr, config, 0, sizeof(pci_config_hdr_t));
}

static int pci_enumerate_is_match(pci_dev_iterator_t *iter)
{
    return (iter->dev_class == -1 ||
            iter->config.dev_class == iter->dev_class) &&
            (iter->subclass == -1 ||
            iter->config.subclass == iter->subclass);
}

int pci_enumerate_next(pci_dev_iterator_t *iter)
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

            // If device is bridge, add bus to todo list
            if (iter->config.dev_class == 0x06 &&
                    iter->config.subclass == 4) {
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

int pci_enumerate_begin(pci_dev_iterator_t *iter,
                        int dev_class, int subclass)
{
    memset(iter, 0, sizeof(*iter));

    iter->dev_class = dev_class;
    iter->subclass = subclass;

    iter->bus = 0;
    iter->slot = 0;
    iter->func = -1;

    iter->header_type = 0;

    iter->bus_todo_len = 0;

    int found;

    while ((found = pci_enumerate_next(iter)) != 0) {
        if (pci_enumerate_is_match(iter))
            break;
    }

    return found;
}

int pci_init(void)
{
#if PCI_DEBUG
    pci_enumerate();
#endif
    return 0;
}

static int pci_enum_capabilities_match(
        uint8_t id, int ofs, uintptr_t context)
{
    if (id == context)
        return ofs;
    return 0;
}

int pci_enum_capabilities(pci_addr_t addr,
                          int (*callback)(uint8_t, int, uintptr_t),
                          uintptr_t context)
{
    int status = pci_config_read(addr, offsetof(pci_config_hdr_t, status),
                                 sizeof(uint16_t));

    if (!(status & PCI_CFG_STATUS_CAPLIST))
        return 0;

    int ofs = pci_config_read(addr, offsetof(pci_config_hdr_t,
                                             capabilities_ptr), 1);

    while (ofs != 0) {
        uint16_t type_next = pci_config_read(addr, ofs, 2);
        uint8_t type = type_next & 0xFF;
        uint16_t next = (type_next >> 8) & 0xFF;

        int result = callback(type, ofs, context);
        if (result != 0)
            return result;

        ofs = next;
    }
    return 0;
}

int pci_find_capability(pci_addr_t addr, int capability_id)
{
    return pci_enum_capabilities(
                addr, pci_enum_capabilities_match, capability_id);
}

bool pci_try_msi_irq(pci_dev_iterator_t const& pci_dev,
                     pci_irq_range_t *irq_range,
                     int cpu, bool distribute, int req_count,
                     intr_handler_t handler, int const *target_cpus)
{
    // Assume we can't use MSI at first, prepare to use pin interrupt
    irq_range->base = pci_dev.config.irq_line;
    irq_range->count = 1;

    bool use_msi = pci_set_msi_irq(pci_dev, irq_range, cpu,
                                   distribute, req_count,
                                   handler, target_cpus);

    if (!use_msi) {
        // Plain IRQ pin
        pci_set_irq_pin(pci_dev.addr, pci_dev.config.irq_pin);
        pci_set_irq_line(pci_dev.addr, pci_dev.config.irq_line);

        irq_hook(pci_dev.config.irq_line, handler);
        irq_setmask(pci_dev.config.irq_line, true);
    }

    return use_msi;
}

bool pci_set_msi_irq(pci_addr_t addr, pci_irq_range_t *irq_range,
                    int cpu, bool distribute, int req_count,
                    intr_handler_t handler, int const *target_cpus)
{
    int capability = 0;
    bool msix = true;

    // Look for the MSI-X extended capability
    capability = pci_find_capability(addr, PCICAP_MSIX);

    if (!capability) {
        // Fall back to MSI
        msix = false;
        capability = pci_find_capability(addr, PCICAP_MSI);
    }

    if (!capability)
        return 0;

    pci_msi_caps_hdr_t caps;

    // Read the header
    pci_config_copy(addr, &caps, capability, sizeof(caps));

    if (msix) {
        // 6.8.3.2 MSI-X configuration

        // "Software must not modify the MSI-X table when any IRQ is unmasked",
        // so we mask the whole function
        PCI_MSIX_MSG_CTRL_MASK_SET(caps.msg_ctrl, 1);
        PCI_MSIX_MSG_CTRL_EN_SET(caps.msg_ctrl, 1);
        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));

        int table_size = PCI_MSIX_MSG_CTRL_TBLSZ_GET(caps.msg_ctrl) + 1;

        uint32_t tbl_pba[2];
        pci_config_copy(addr, tbl_pba,
                        capability + PCI_MSIX_TBL, sizeof(tbl_pba));

        // See which BIR it uses initially
        int tbl_bir = PCI_MSIX_TBL_BIR_GET(tbl_pba[0]);

        size_t tbl_sz = sizeof(pci_msix64_t) * table_size;
        uint32_t pba_sz = ((table_size + 63) & -64) >> 3;

        uint64_t tbl_physaddr = mm_alloc_hole(tbl_sz + pba_sz);

        pci_msix64_t volatile *table = (pci_msix64_t*)
                mmap((void*)tbl_physaddr, tbl_sz + pba_sz,
                     PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

        pcix_tables.emplace_back(addr, table);

        // Both table and PBA use same BIR, set PBA offset to after table
        PCI_MSIX_TBL_OFS_SET(tbl_pba[0], 0);
        PCI_MSIX_TBL_BIR_SET(tbl_pba[0], tbl_bir);
        PCI_MSIX_TBL_OFS_SET(tbl_pba[1], (tbl_sz) >> 3);
        PCI_MSIX_TBL_BIR_SET(tbl_pba[1], tbl_bir);

        // Write table and PBA configuration
        pci_config_write(addr, capability + PCI_MSIX_TBL,
                         tbl_pba, sizeof(tbl_pba));

        // Set BAR
        pci_config_write(addr, offsetof(pci_config_hdr_t, base_addr) +
                         tbl_bir * sizeof(uint32_t),
                         &tbl_physaddr, sizeof(tbl_physaddr));

        // Set enable (but still masked)
        PCI_MSIX_MSG_CTRL_EN_SET(caps.msg_ctrl, 1);

        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));

        int tbl_cnt = min(req_count, table_size);

        vector<msi_irq_mem_t> msi_writes(tbl_cnt);

        irq_range->base = apic_msi_irq_alloc(
                    msi_writes.data(), tbl_cnt, cpu, distribute,
                    handler, target_cpus);
        irq_range->count = tbl_cnt;

        int i;
        for (i = 0; i != tbl_cnt; ++i) {
            msi_irq_mem_t const& write = msi_writes[i];
            table[i].addr = write.addr;
            table[i].data = write.data;
            // 0 = Not masked
            table[i].ctrl = 0;
        }

        while (i < table_size) {
            table[i].addr = 0;
            table[i].data = 0;
            // 1 = masked
            table[i].ctrl = 1;
            ++i;
        }

        // Unmask function
        PCI_MSIX_MSG_CTRL_MASK_SET(caps.msg_ctrl, 0);

        pci_config_write(addr,
                         capability + offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                         &caps.msg_ctrl, sizeof(caps.msg_ctrl));
    } else {
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
        irq_range->base = apic_msi_irq_alloc(
                    mem, irq_range->count,
                    cpu, distribute,
                    handler);

        // Use 32-bit or 64-bit according to capability
        if (caps.msg_ctrl & PCI_MSI_MSG_CTRL_CAP64) {
            // 64 bit address
            pci_msi64_t cfg;
            cfg.addr_lo = (uint32_t)mem[0].addr;
            cfg.addr_hi = (uint32_t)((uint64_t)mem[0].addr >> 32);
            cfg.data = (uint16_t)mem[0].data;

            pci_config_write(addr, capability + sizeof(caps),
                             &cfg, sizeof(cfg));
        } else {
            // 32 bit address
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

void pci_adj_control_bits(pci_dev_t const& pci_dev,
                          uint16_t set, uint16_t clr)
{
    pci_adj_control_bits(pci_dev.addr, set, clr);
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
