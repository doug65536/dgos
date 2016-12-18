#include "pci.h"
#include "cpu/ioport.h"
#include "cpu/spinlock.h"
#include "printk.h"
#include "string.h"

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

#if PCI_DEBUG
static char const *pci_device_class_text[] = {
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
#endif

uint32_t pci_read_config(
        int bus, int slot, int func,
        int offset, int size)
{
    spinlock_lock(&pci_spinlock);

    uint32_t data = ~(uint32_t)0;

    if (bus < 256 && slot < 32 &&
            func < 8 && offset < 256) {
        uint32_t pci_address = (1 << 31) |
                (bus << 16) |
                (slot << 11) |
                (func << 8) |
                (offset & ~(uint32_t)3);

        outd(PCI_ADDR, pci_address);
        data = ind(PCI_DATA);

        data >>= (offset & 3) << 3;

        if (size != sizeof(uint32_t))
            data &= ~(~(uint32_t)0 << (size << 3));
    }

    spinlock_unlock(&pci_spinlock);

    return data;
}

uint32_t pci_write_config(
        int bus, int slot, int func,
        int offset, int value)
{
    spinlock_lock(&pci_spinlock);

    uint32_t data = ~(uint32_t)0;

    if (bus < 256 && slot < 32 &&
            func < 8 && offset < 256) {
        uint32_t pci_address = (1 << 31) |
                (bus << 16) |
                (slot << 11) |
                (func << 8) |
                (offset & ~(uint32_t)3);

        outd(PCI_ADDR, pci_address);
        outd(PCI_DATA, value);

        outd(PCI_ADDR, pci_address);
        data = ind(PCI_DATA);
    }

    spinlock_unlock(&pci_spinlock);

    return data;
}

#if PCI_DEBUG
#define PCI_DEFINE_CONFIG_GETTER(type, field) \
    static uint16_t pci_device_##field ( \
            uint32_t bus, uint32_t slot, uint32_t func) \
    { \
        return pci_read_config( \
                    bus, slot, func, \
                    offset_of(type, field), \
                    size_of(type, field)); \
    }

PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, vendor)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, dev_class)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, subclass)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, header_type)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, device)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, irq_line)

static void pci_enumerate(void)
{
    PCI_DEBUGMSG("Enumerating PCI devices\n");
    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor = pci_device_vendor(bus, slot, 0);

            if (vendor == 0xFFFF)
                continue;

            uint16_t header_type = pci_device_header_type(
                        bus, slot, 0);

            uint32_t function_count = (header_type & 0x80) ? 8 : 1;

            for (uint32_t func = 0; func < function_count; ++func) {
                uint8_t dev_class = pci_device_dev_class(
                            bus, slot, func);

                uint16_t device = pci_device_device(bus, slot, func);

                if (dev_class == 0xFF)
                    break;

                uint16_t subclass = pci_device_subclass(bus, slot, func);

                char const *class_text = "<unknown>";
                if (dev_class < countof(pci_device_class_text))
                    class_text = pci_device_class_text[dev_class];

                uint8_t irq = pci_device_irq_line(bus, slot, func);

                PCI_DEBUGMSG("Found device, vendor=%04x device=%04x irq=%d\n"
                             " bus=%d,"
                             " slot=%d,"
                             " func=%d,"
                             " class=%s"
                             " subclass=%x"
                             " (%d)\n",
                             vendor, device, irq,
                             bus,
                             slot,
                             func,
                             class_text,
                             subclass,
                             dev_class);

                if (vendor == 0x10EC && device == 0x8139) {
                    // RTL 8139
                    PCI_DEBUGMSG("RTL8139 NIC\n");
                    for (int i = 0; i < 6; ++i) {
                        uint32_t base_addr = pci_read_config(
                                    bus, slot, func,
                                    offset_of(pci_config_hdr_t, base_addr) +
                                    sizeof(uint32_t) * i,
                                    sizeof(uint32_t));

                        PCI_DEBUGMSG("base_addr[%d] = %x\n", i, base_addr);
                    }
                }
            }
        }
    }
}
#endif

static void pci_enumerate_read(pci_config_hdr_t *config,
                               int bus, int slot, int func)
{
    uint32_t values[16];

    for (int ofs = 0; ofs < 16; ++ofs)
        values[ofs] = pci_read_config(bus, slot, func,
                                      ofs << 2, sizeof(uint32_t));

    memcpy(config, values, sizeof(*config));
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
            pci_enumerate_read(&iter->config,
                               iter->bus, iter->slot, iter->func);

            // Capture header type on function 0
            if (iter->func == 0)
                iter->header_type = iter->config.header_type;

            // If device is not there, then next
            if (iter->config.dev_class == (uint8_t)~0 &&
                    iter->config.vendor == (uint16_t)~0)
                continue;

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
            if (++iter->bus >= 256) {
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
