#include "pci.h"
#include "cpu/ioport.h"
#include "cpu/spinlock.h"
#include "printk.h"
#include "string.h"
//#include "irq.h"
#include "cpu/apic.h"

#define PCI_DEBUG   0
#if PCI_DEBUG
#define PCI_DEBUGMSG(...) printk(__VA_ARGS__)
#else
#define PCI_DEBUGMSG(...) ((void)0)
#endif

static spinlock_t pci_spinlock;

#define PCI_ADDR    0xCF8
#define PCI_DATA    0xCFC

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

uint32_t pci_config_read(
        int bus, int slot, int func,
        int offset, int size)
{

    uint32_t data = ~(uint32_t)0;

    if (bus < 256 && slot < 32 &&
            func < 8 && offset < 256) {
        uint32_t pci_address = (1 << 31) |
                (bus << 16) |
                (slot << 11) |
                (func << 8) |
                (offset & -4);

        spinlock_lock_noirq(&pci_spinlock);

        outd(PCI_ADDR, pci_address);
        data = ind(PCI_DATA);

        spinlock_unlock_noirq(&pci_spinlock);

        data >>= (offset & 3) << 3;

        if (size != sizeof(uint32_t))
            data &= ~((uint32_t)-1 << (size << 3));
    }

    return data;
}

int pci_config_write(
        int bus, int slot, int func,
        size_t offset, void *values, size_t size)
{
    // Validate
    if (!(bus >= 0 && bus < 256 &&
          slot >= 0 && slot < 32 &&
          func >= 0 && func < 8 &&
          offset < 256 && size <= 256 &&
          offset + size <= 256))
        return 0;

    // Pointer to input data
    char *p = (char*)values;

    uint32_t pci_address = (1 << 31) |
            (bus << 16) |
            (slot << 11) |
            (func << 8);

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

void pci_config_copy(int bus, int slot, int func,
                     void *dest, int ofs, size_t size)
{
    uint32_t value;
    char *out = (char*)dest;

    for (size_t i = 0; i < size; i += sizeof(uint32_t)) {
        value = pci_config_read(bus, slot, func,
                                ofs + i, sizeof(value));

        if (i + sizeof(value) <= size)
            memcpy(out + i, &value, sizeof(value));
        else
            memcpy(out + i, &value, size - i);
    }
}

static void pci_enumerate_read(int bus, int slot, int func,
                               pci_config_hdr_t *config)
{
    pci_config_copy(bus, slot, func,
                    config, 0, sizeof(pci_config_hdr_t));
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
            pci_enumerate_read(iter->bus, iter->slot, iter->func,
                               &iter->config);

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
                    printdbg("Too many PCI bridges! Droppped one\n");
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

int pci_enum_capabilities(int bus, int slot, int func,
                          int (*callback)(uint8_t, int, uintptr_t),
                          uintptr_t context)
{
    int status = pci_config_read(bus, slot, func,
                                 offsetof(pci_config_hdr_t, status),
                                 sizeof(uint16_t));

    if (!(status & PCI_CFG_STATUS_CAPLIST))
        return 0;

    int ofs = pci_config_read(bus, slot, func,
                              offsetof(pci_config_hdr_t,
                                       capabilities_ptr), 1);

    while (ofs != 0) {
        uint16_t type_next = pci_config_read(bus, slot, func, ofs, 2);
        uint8_t type = type_next & 0xFF;
        uint16_t next = (type_next >> 8) & 0xFF;

        int result = callback(type, ofs, context);
        if (result != 0)
            return result;

        ofs = next;
    }
    return 0;
}

int pci_find_capability(int bus, int slot, int func,
                        int capability_id)
{
    return pci_enum_capabilities(
                bus, slot, func,
                pci_enum_capabilities_match, capability_id);
}

bool pci_try_msi_irq(pci_dev_t const& pci_dev,
                     pci_irq_range_t *irq_range,
                     int cpu, bool distribute, int multiple,
                     intr_handler_t handler)
{
    // Assume we can't use MSI at first, prepare to use pin interrupt
    irq_range->base = pci_dev.config.irq_line;
    irq_range->count = 1;

    bool use_msi = pci_set_msi_irq(
                pci_dev.bus, pci_dev.slot, pci_dev.func,
                irq_range, cpu, distribute, multiple, handler);

    if (!use_msi) {
        // Plain IRQ pin
        pci_set_irq_pin(pci_dev.bus, pci_dev.slot,
                        pci_dev.func,
                        pci_dev.config.irq_pin);
        pci_set_irq_line(pci_dev.bus, pci_dev.slot,
                         pci_dev.func,
                         pci_dev.config.irq_line);

        irq_hook(pci_dev.config.irq_line, handler);
        irq_setmask(pci_dev.config.irq_line, true);
    }

    return use_msi;
}

bool pci_set_msi_irq(int bus, int slot, int func,
                    pci_irq_range_t *irq_range,
                    int cpu, bool distribute, int multiple,
                    intr_handler_t handler)
{
    // Look for the MSI extended capability
    int msicap_config = pci_find_capability(
                bus, slot, func, PCICAP_MSI);

    if (msicap_config == 0)
        return 0;

    pci_msi_caps_hdr_t caps;

    // Read the header
    pci_config_copy(bus, slot, func, &caps,
                    msicap_config, sizeof(caps));

    // Sanity check capability ID
    assert(caps.capability_id == PCICAP_MSI);

    // Extract the multi message capability value
    uint8_t multi_exp = (caps.msg_ctrl >> PCI_MSI_HDR_MMC_BIT) &
            PCI_MSI_HDR_MMC_MASK;
    uint8_t multi_cap = 1U << multi_exp;

    // Sanity check multi message capability
    assert(multi_cap <= 32);

    // Limit it to highest documented value (as at PCI v2.2)
    if (unlikely(multi_cap > 32))
        multi_cap = 32;

    uint8_t multi_en = multiple ? multi_exp : 0;

    caps.msg_ctrl = (caps.msg_ctrl & ~PCI_MSI_HDR_MME) |
            PCI_MSI_HDR_MME_n(multi_en) |
            PCI_MSI_HDR_EN;

    // Allocate IRQs
    msi_irq_mem_t mem[32];
    irq_range->count = 1 << multi_en;
    irq_range->base = apic_msi_irq_alloc(
                mem, irq_range->count,
                cpu, distribute,
                handler);

    // Use 32-bit or 64-bit according to capability
    if (caps.msg_ctrl & PCI_MSI_HDR_64) {
        // 64 bit address
        pci_msi64_t cfg;
        cfg.addr_lo = (uint32_t)mem[0].addr;
        cfg.addr_hi = (uint32_t)((uint64_t)mem[0].addr >> 32);
        cfg.data = (uint16_t)mem[0].data;

        pci_config_write(bus, slot, func,
                         msicap_config + sizeof(caps),
                         &cfg, sizeof(cfg));
    } else {
        // 32 bit address
        pci_msi32_t cfg;
        cfg.addr = (uint32_t)mem[0].addr;
        cfg.data = (uint16_t)mem[0].data;

        pci_config_write(bus, slot, func,
                         msicap_config + sizeof(caps),
                         &cfg, sizeof(cfg));
    }

    // Write msg_ctrl
    pci_config_write(bus, slot, func, msicap_config +
                     offsetof(pci_msi_caps_hdr_t, msg_ctrl),
                     &caps.msg_ctrl, sizeof(caps.msg_ctrl));

    return true;
}

void pci_set_irq_line(int bus, int slot, int func,
                     uint8_t irq_line)
{
    pci_config_write(bus, slot, func,
                     offsetof(pci_config_hdr_t, irq_line),
                     &irq_line, sizeof(irq_line));
}

void pci_set_irq_pin(int bus, int slot, int func,
                     uint8_t irq_pin)
{
    pci_config_write(bus, slot, func,
                     offsetof(pci_config_hdr_t, irq_pin),
                     &irq_pin, sizeof(irq_pin));
}

static void pci_adj_bits_16(int bus, int slot, int func,
                            int offset,
                            uint16_t set, uint16_t clr)
{
    if (set || clr) {
        uint16_t reg;
        uint16_t new_reg;

        pci_config_copy(bus, slot, func, &reg, offset,
                        sizeof(reg));

        new_reg = (reg | set) & ~clr;

        if (new_reg != reg) {
            pci_config_write(bus, slot, func, offset,
                             &new_reg, sizeof(new_reg));
        }
    }
}

void pci_adj_control_bits(pci_dev_t const& pci_dev,
                          uint16_t set, uint16_t clr)
{
    pci_adj_control_bits(pci_dev.bus, pci_dev.slot, pci_dev.func, set, clr);
}

void pci_adj_control_bits(int bus, int slot, int func,
                          uint16_t set, uint16_t clr)
{
    pci_adj_bits_16(bus, slot, func,
                    offsetof(pci_config_hdr_t, command),
                    set, clr);
}

void pci_clear_status_bits(int bus, int slot, int func,
                           uint16_t bits)
{
    pci_config_write(bus, slot, func,
                     offsetof(pci_config_hdr_t, status),
                     &bits, sizeof(bits));
}

char const *pci_device_class_text(uint8_t cls)
{
    if (cls < sizeof(pci_device_class_text_lookup))
        return pci_device_class_text_lookup[cls];
    return nullptr;
}
