#pragma once
#include "types.h"
#include "assert.h"
#include "irq.h"

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

C_ASSERT(offsetof(pci_config_hdr_t, capabilities_ptr) == 0x34);

#define PCI_CFG_STATUS_CAPLIST_BIT  4
#define PCI_CFG_STATUS_CAPLIST      (1<<PCI_CFG_STATUS_CAPLIST_BIT)

typedef struct pci_dev_iterator_t {
    pci_config_hdr_t config;

    int dev_class;
    int subclass;

    int bus;
    int slot;
    int func;

    uint8_t header_type;

    uint8_t bus_todo_len;
    uint8_t bus_todo[64];
} pci_dev_iterator_t;

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

typedef struct pci_msi_hdr_t {
    uint8_t capability_id;
    uint8_t next_ptr;
    uint16_t msg_ctrl;
} pci_msi_caps_hdr_t;

// 64-bit capable
#define PCI_MSI_HDR_64_BIT      7

// Multiple Message Enable
#define PCI_MSI_HDR_MME_BIT     4
#define PCI_MSI_HDR_MME_BITS    3

// Multiple Message Capable (log2 N)
#define PCI_MSI_HDR_MMC_BIT     4
#define PCI_MSI_HDR_MMC_BITS    3

// Enable
#define PCI_MSI_HDR_EN_BIT      0

#define PCI_MSI_HDR_MMC_MASK    ((1<<PCI_MSI_HDR_MMC_BITS)-1)
#define PCI_MSI_HDR_MME_MASK    ((1<<PCI_MSI_HDR_MME_BITS)-1)

#define PCI_MSI_HDR_64          (1<<PCI_MSI_HDR_64_BIT)
#define PCI_MSI_HDR_EN          (1<<PCI_MSI_HDR_EN_BIT)

#define PCI_MSI_HDR_MMC         (PCI_MSI_HDR_MMC_MASK<<PCI_MSI_HDR_MMC_BIT)
#define PCI_MSI_HDR_MME         (PCI_MSI_HDR_MME_MASK<<PCI_MSI_HDR_MME_BIT)

#define PCI_MSI_HDR_MMC_n(n)    ((n)<<PCI_MSI_HDR_MMC_BIT)
#define PCI_MSI_HDR_MME_n(n)    ((n)<<PCI_MSI_HDR_MME_BIT)

typedef struct pci_msi32_t {
    uint32_t addr;
    uint16_t data;
} __attribute__((packed)) pci_msi32_t;

typedef struct pci_msi64_t {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint16_t data;
} __attribute__((packed)) pci_msi64_t;

typedef struct pci_irq_range_t {
    uint8_t base;
    uint8_t count;
} pci_irq_range_t;

int pci_set_msi_irq(int bus, int slot, int func,
                    pci_irq_range_t *irq_range,
                    int cpu, int distribute, int multiple,
                    intr_handler_t handler);
