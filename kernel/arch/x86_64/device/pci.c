#include "pci.h"
#include "cpu/ioport.h"
#include "printk.h"

#define PCI_ADDR    0xCF8
#define PCI_DATA    0xCFC

#define offset_of(type, member) \
    ((uintptr_t)&(((type*)0x10U)->member) - 0x10U)
#define size_of(type, member) \
    sizeof(((type*)0x10U)->member)

typedef struct pci_config_hdr_t {
    uint16_t vendor;
    uint16_t device;

    uint16_t command;
    uint16_t status;

    uint8_t revision;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t dev_class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;

    uint32_t base_addr[6];

    uint32_t cardbus_cis_ptr;

    uint16_t subsystem_vendor;
    uint16_t subsystem_id;

    uint32_t expansion_rom_addr;
    uint8_t capabilities_ptr;

    uint8_t reserved[7];

    uint8_t irq_line;
    uint8_t irq_pin;
    uint8_t min_grant;
    uint8_t max_latency;
} pci_config_hdr_t;

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

static uint32_t pci_read_config(
        uint32_t bus, uint32_t slot, uint32_t func,
        uint32_t offset, uint32_t size)
{
    if (bus < 256 && slot < 32 &&
            func < 8 && offset < 256) {
        uint32_t pci_address = (1 << 31) |
                (bus << 16) |
                (slot << 11) |
                (func << 8) |
                (offset & ~(uint32_t)3);

        outd(PCI_ADDR, pci_address);
        uint32_t data = ind(PCI_DATA);

        data >>= (offset & 3) << 3;

        if (size != sizeof(uint32_t))
            data &= ~(~(uint32_t)0 << (size << 3));

        return data;
    }
    return ~0U;
}

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
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, header_type)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, device)
PCI_DEFINE_CONFIG_GETTER(pci_config_hdr_t, irq_line)

static void pci_enumerate(void)
{
    printk("Enumerating PCI devices\n");
    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor = pci_device_vendor(bus, slot, 0);

            //printk("Checking PCI bus=%u slot=%u", bus, slot);

            if (vendor == 0xFFFF)
                continue;

            uint16_t header_type = pci_device_header_type(
                        bus, slot, 0);

            uint32_t function_count = (header_type & 0x80) ? 8 : 1;

            for (uint32_t func = 0; func < function_count; ++func) {
                uint8_t dev_class = pci_device_dev_class(
                            bus, slot, func);

                uint16_t device = pci_device_device(bus, slot, 0);

                if (dev_class == 0xFF)
                    break;

                char const *class_text = "<unknown>";
                if (dev_class < countof(pci_device_class_text))
                    class_text = pci_device_class_text[dev_class];

                uint8_t irq = pci_device_irq_line(bus, slot, func);

                printk("Found device, vendor=%04x device=%04x irq=%d\n"
                       " bus=%d,"
                       " slot=%d,"
                       " func=%d,"
                       " class=%s"
                       " (%d)\n",
                       vendor, device, irq,
                       bus,
                       slot,
                       func,
                       class_text,
                       dev_class);

                if (vendor == 0x10EC && device == 0x8139) {
                    // RTL 8139
                    printk("RTL8139 NIC\n");
                    for (int i = 0; i < 6; ++i) {
                        uint32_t base_addr = pci_read_config(
                                    bus, slot, func,
                                    offset_of(pci_config_hdr_t, base_addr) +
                                    sizeof(uint32_t) * i,
                                    sizeof(uint32_t));

                        printk("base_addr[%d] = %x\n", i, base_addr);
                    }
                }
            }
        }
    }
}

int pci_init(void)
{
    pci_enumerate();
    return 0;
}
