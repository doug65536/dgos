#include "dev_storage.h"

#include "cpu/ioport.h"
#include "pci.h"
#include "mm.h"
#include "string.h"
#include "time.h"

DECLARE_storage_if_DEVICE(ide);

typedef struct ide_chan_ports_t {
    ioport_t cmd;
    ioport_t ctl;
    uint8_t irq;
} ide_chan_ports_t;

struct storage_if_t {
    storage_if_vtbl_t *vtbl;

    ide_chan_ports_t ports[2];
    ioport_t bmdma_base;
};

#define IDE_MAX_IF 8
static storage_if_t ide_if[IDE_MAX_IF];
static size_t ide_if_count;

static int ide_detect(storage_if_base_t **result)
{
    pci_dev_iterator_t iter;

    if (pci_enumerate_begin(&iter, 1, 1)) {
        do {
            storage_if_t *dev = ide_if + ide_if_count++;

            dev->ports[0].cmd = (iter.config.base_addr[0] <= 1)
                    ? 0x1F0
                    : iter.config.base_addr[0];

            dev->ports[0].ctl = (iter.config.base_addr[1] <= 1)
                    ? 0x3F6
                    : iter.config.base_addr[1];

            dev->ports[1].cmd = (iter.config.base_addr[2] <= 1)
                    ? 0x170
                    : iter.config.base_addr[2];

            dev->ports[1].ctl = (iter.config.base_addr[3] <= 1)
                    ? 0x376
                    : iter.config.base_addr[3];

            dev->bmdma_base = iter.config.base_addr[4];

            if (iter.config.prog_if == 0x8A ||
                    iter.config.prog_if == 0x80) {
                dev->ports[0].irq = 14;
                dev->ports[1].irq = 15;
            } else if (iter.config.irq_line != 0xFE) {
                dev->ports[0].irq = iter.config.irq_line;
                dev->ports[1].irq = iter.config.irq_line;
            } else {
                dev->ports[0].irq = 14;
                dev->ports[1].irq = 14;
                pci_write_config(iter.bus, iter.slot, iter.func,
                               offsetof(pci_config_hdr_t, irq_line),
                               14);
            }
        } while (pci_enumerate_next(&iter));
    }

    if (ide_if_count > 0)
        *result = (storage_if_base_t*)ide_if;

    return ide_if_count > 0;
}

static void ide_init(storage_if_base_t *i)
{
    (void)i;
}

static void ide_cleanup(storage_if_base_t *i)
{
    (void)i;
}

static int ide_detect_devices(storage_dev_base_t **result)
{
    (void)result;
    return 0;
}

REGISTER_storage_if_DEVICE(ide)
