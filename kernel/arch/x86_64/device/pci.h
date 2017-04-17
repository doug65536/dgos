#pragma once
#include "types.h"
#include "assert.h"
#include "irq.h"

struct pci_config_hdr_t {
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
};

// PCI config command register
#define PCI_CMD_IOEN_BIT            0
#define PCI_CMD_MEMEN_BIT           1
#define PCI_CMD_BUSMASTER_BIT       2
#define PCI_CMD_SPECIALCYCLE_BIT    3
#define PCI_CMD_MEMWRINVEN_BIT      4
#define PCI_CMD_VGASNOOP_BIT        5
#define PCI_CMD_PARITYERR_BIT       6
#define PCI_CMD_STEPPING_BIT        7
#define PCI_CMD_SERR_BIT            8
#define PCI_CMD_FASTB2B_BIT         9

#define PCI_CMD_IOEN                (1<<PCI_CMD_IOEN_BIT)
#define PCI_CMD_MEMEN               (1<<PCI_CMD_MEMEN_BIT)
#define PCI_CMD_BUSMASTER           (1<<PCI_CMD_BUSMASTER_BIT)
#define PCI_CMD_SPECIALCYCLE        (1<<PCI_CMD_SPECIALCYCLE_BIT)
#define PCI_CMD_MEMWRINVEN          (1<<PCI_CMD_MEMWRINVEN_BIT)
#define PCI_CMD_VGASNOOP            (1<<PCI_CMD_VGASNOOP_BIT)
#define PCI_CMD_PARITYERR           (1<<PCI_CMD_PARITYERR_BIT)
#define PCI_CMD_STEPPING            (1<<PCI_CMD_STEPPING_BIT)
#define PCI_CMD_SERR                (1<<PCI_CMD_SERR_BIT)
#define PCI_CMD_FASTB2B             (1<<PCI_CMD_FASTB2B_BIT)

#define PCI_DEV_CLASS_UNCLASSIFIED      0x00
#define PCI_DEV_CLASS_STORAGE           0x01
#define PCI_DEV_CLASS_NETWORK           0x02
#define PCI_DEV_CLASS_DISPLAY           0x03
#define PCI_DEV_CLASS_MULTIMEDIA        0x04
#define PCI_DEV_CLASS_MEMORY            0x05
#define PCI_DEV_CLASS_BRIDGE            0x06
#define PCI_DEV_CLASS_COMMUNICATION     0x07
#define PCI_DEV_CLASS_SYSTEM            0x08
#define PCI_DEV_CLASS_INPUT             0x09
#define PCI_DEV_CLASS_DOCKING           0x0A
#define PCI_DEV_CLASS_PROCESSOR         0x0B
#define PCI_DEV_CLASS_SERIAL            0x0C
#define PCI_DEV_CLASS_WIRELESS          0x0D
#define PCI_DEV_CLASS_INTELLIGENT       0x0E
#define PCI_DEV_CLASS_SATELLITE         0x0F
#define PCI_DEV_CLASS_ENCRYPTION        0x10
#define PCI_DEV_CLASS_DSP               0x11
#define PCI_DEV_CLASS_ACCELERATOR       0x12
#define PCI_DEV_CLASS_INSTRUMENTATION   0x13
#define PCI_DEV_CLASS_COPROCESSOR       0x40
#define PCI_DEV_CLASS_UNASSIGNED        0xFF

#define PCI_SUBCLASS_NETWORK_ETHERNET   0x00

#define PCI_SUBCLASS_STORAGE_SCSI       0x00
#define PCI_SUBCLASS_STORAGE_IDE        0x01
#define PCI_SUBCLASS_STORAGE_FLOPPY     0x02
#define PCI_SUBCLASS_STORAGE_IPIBUS     0x03
#define PCI_SUBCLASS_STORAGE_RAID       0x04
#define PCI_SUBCLASS_STORAGE_ATA        0x05
#define PCI_SUBCLASS_STORAGE_SATA       0x06
#define PCI_SUBCLASS_STORAGE_SAS        0x07
#define PCI_SUBCLASS_STORAGE_NVM        0x08
#define PCI_SUBCLASS_STORAGE_MASS       0x80

#define PCI_SUBCLASS_SERIAL_IEEE1394    0x00
#define PCI_SUBCLASS_SERIAL_ACCESS      0x01
#define PCI_SUBCLASS_SERIAL_SSA         0x02
#define PCI_SUBCLASS_SERIAL_USB         0x03
#define PCI_SUBCLASS_SERIAL_FIBRECHAN   0x04
#define PCI_SUBCLASS_SERIAL_SMBUS       0x05
#define PCI_SUBCLASS_SERIAL_INFINIBAND  0x06
#define PCI_SUBCLASS_SERIAL_IPMISMIC    0x07
#define PCI_SUBCLASS_SERIAL_SERCOS      0x08
#define PCI_SUBCLASS_SERIAL_CANBUS      0x09

#define PCI_PROGIF_STORAGE_SATA_VEND    0x00
#define PCI_PROGIF_STORAGE_SATA_AHCI    0x01
#define PCI_PROGIF_STORAGE_SATA_SERIAL  0x02

#define PCI_PROGIF_SERIAL_USB_UHCI      0x00
#define PCI_PROGIF_SERIAL_USB_OHCI      0x10
#define PCI_PROGIF_SERIAL_USB_EHCI      0x20
#define PCI_PROGIF_SERIAL_USB_XHCI      0x30
#define PCI_PROGIF_SERIAL_USB_UNSPEC    0x80
#define PCI_PROGIF_SERIAL_USB_USBDEV    0xFE

C_ASSERT(offsetof(pci_config_hdr_t, capabilities_ptr) == 0x34);

#define PCI_CFG_STATUS_CAPLIST_BIT  4
#define PCI_CFG_STATUS_CAPLIST      (1<<PCI_CFG_STATUS_CAPLIST_BIT)

struct pci_dev_iterator_t {
    pci_config_hdr_t config;

    int dev_class;
    int subclass;

    int bus;
    int slot;
    int func;

    uint8_t header_type;

    uint8_t bus_todo_len;
    uint8_t bus_todo[64];
};

int pci_init(void);

int pci_enumerate_begin(pci_dev_iterator_t *iter,
                        int dev_class, int subclass);
int pci_enumerate_next(pci_dev_iterator_t *iter);

uint32_t pci_config_read(
        int bus, int slot, int func,
        int offset, int size);

int pci_config_write(int bus, int slot, int func,
        size_t offset, void *values, size_t size);

int pci_find_capability(
        int bus, int slot, int func,
        int capability_id);

int pci_enum_capabilities(int bus, int slot, int func,
        int (*callback)(uint8_t, int, uintptr_t), uintptr_t context);

//
// PCI capability IDs

// Power management
#define PCICAP_PM       1

// Accelerated Graphics Port
#define PCICAP_AGP      2

// Vital Product Data
#define PCICAP_VPD      3

// Slot Identification (external expansion)
#define PCICAP_SLOTID   4

// Message Signaled Interrupts
#define PCICAP_MSI      5

// CompactPCI Hotswap
#define PCICAP_HOTSWAP  6

//
// MSI

struct pci_msi_caps_hdr_t {
    uint8_t capability_id;
    uint8_t next_ptr;
    uint16_t msg_ctrl;
};

// 64-bit capable
#define PCI_MSI_HDR_64_BIT      7

// Multiple Message Enable
#define PCI_MSI_HDR_MME_BIT     4
#define PCI_MSI_HDR_MME_BITS    3

// Multiple Message Capable (log2 N)
#define PCI_MSI_HDR_MMC_BIT     1
#define PCI_MSI_HDR_MMC_BITS    3

// Enable
#define PCI_MSI_HDR_EN_BIT      0

#define PCI_MSI_HDR_MMC_MASK    ((1U<<PCI_MSI_HDR_MMC_BITS)-1)
#define PCI_MSI_HDR_MME_MASK    ((1U<<PCI_MSI_HDR_MME_BITS)-1)

#define PCI_MSI_HDR_64          (1U<<PCI_MSI_HDR_64_BIT)
#define PCI_MSI_HDR_EN          (1U<<PCI_MSI_HDR_EN_BIT)

#define PCI_MSI_HDR_MMC         (PCI_MSI_HDR_MMC_MASK<<PCI_MSI_HDR_MMC_BIT)
#define PCI_MSI_HDR_MME         (PCI_MSI_HDR_MME_MASK<<PCI_MSI_HDR_MME_BIT)

#define PCI_MSI_HDR_MMC_n(n)    ((n)<<PCI_MSI_HDR_MMC_BIT)
#define PCI_MSI_HDR_MME_n(n)    ((n)<<PCI_MSI_HDR_MME_BIT)

struct pci_msi32_t {
    uint32_t addr;
    uint16_t data;
} __packed;

struct pci_msi64_t {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint16_t data;
} __packed;

struct pci_irq_range_t {
    uint8_t base;
    uint8_t count;
};

bool pci_set_msi_irq(int bus, int slot, int func,
                    pci_irq_range_t *irq_range,
                    int cpu, int distribute, int multiple,
                    intr_handler_t handler);

void pci_set_irq_line(int bus, int slot, int func,
                      uint8_t irq_line);

void pci_set_irq_pin(int bus, int slot, int func,
                     uint8_t irq_pin);

void pci_adj_control_bits(int bus, int slot, int func,
                          uint16_t set, uint16_t clr);

void pci_clear_status_bits(int bus, int slot, int func,
                           uint16_t bits);
