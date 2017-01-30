#pragma once
#include "types.h"

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

uint32_t pci_read_config(
        int bus, int slot, int func,
        int offset, int size);

uint32_t pci_write_config(
        int bus, int slot, int func,
        int offset, int value);
