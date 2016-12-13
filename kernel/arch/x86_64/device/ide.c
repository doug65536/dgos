#include "dev_storage.h"

#include "cpu/ioport.h"
#include "pci.h"
#include "mm.h"
#include "string.h"
#include "time.h"
#include "printk.h"

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

//
// Status

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Inlex
#define ATA_SR_ERR     0x01    // Error

//
// Errors

#define ATA_ER_BBK      0x80    // Bad sector
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // No media
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // No media
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

//
// Commands

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

//
// ATAPI commands

#define      ATAPI_CMD_READ       0xA8
#define      ATAPI_CMD_EJECT      0x1B

static int ide_detect(storage_if_base_t **result)
{
    pci_dev_iterator_t iter;

    if (!pci_enumerate_begin(&iter, 1, 1))
        return 0;

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

    if (ide_if_count > 0)
        *result = (storage_if_base_t*)ide_if;

    return ide_if_count;
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
