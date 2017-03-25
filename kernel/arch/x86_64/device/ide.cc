#define STORAGE_IMPL
#define STORAGE_DEV_NAME ide
#include "dev_storage.h"
#undef STORAGE_DEV_NAME

#include "cpu/ioport.h"
#include "pci.h"
#include "mm.h"
#include "string.h"
#include "time.h"
#include "printk.h"

struct ide_chan_ports_t {
    ioport_t cmd;
    ioport_t ctl;
    uint8_t irq;
};

struct ide_if_t : public storage_if_base_t {
    STORAGE_IF_IMPL

    ide_chan_ports_t ports[2];
    ioport_t bmdma_base;
};

#define IDE_MAX_IF 8
static ide_if_t ide_ifs[IDE_MAX_IF];
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

//
// Task file

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

if_list_t ide_if_t::detect(void)
{
    unsigned start_at = ide_if_count;

    if_list_t list = {
        ide_ifs + start_at,
        sizeof(*ide_ifs),
        0
    };

    pci_dev_iterator_t iter;

    if (!pci_enumerate_begin(
                &iter,
                PCI_DEV_CLASS_STORAGE,
                PCI_SUBCLASS_STORAGE_IDE))
        return list;

    static ioport_t std_ports[] = {
        0x1F0, 0x3F6,
        0x170, 0x376,
        0x1E8, 0x3EE,
        0x168, 0x36E
    };

    size_t std_idx = 0;

    do {
        ide_if_t *dev = ide_ifs + ide_if_count++;

        dev->ports[0].cmd = (iter.config.base_addr[0] > 1)
                ? iter.config.base_addr[0]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->ports[0].ctl = (iter.config.base_addr[1] > 1)
                ? iter.config.base_addr[1]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->ports[1].cmd = (iter.config.base_addr[2] > 1)
                ? iter.config.base_addr[2]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

        dev->ports[1].ctl = (iter.config.base_addr[3] > 1)
                ? iter.config.base_addr[3]
                : std_idx < countof(std_ports)
                ? std_ports[std_idx++]
                : 0;

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
            pci_config_write(iter.bus, iter.slot, iter.func,
                           offsetof(pci_config_hdr_t, irq_line),
                           &dev->ports[0].irq, sizeof(uint8_t));
        }

        // Sanity check ports, reject entry if any are zero
        if (dev->ports[0].cmd == 0 || dev->ports[0].ctl == 0 ||
                dev->ports[1].cmd == 0 || dev->ports[1].ctl == 0) {
            --ide_if_count;
            continue;
        }


    } while (pci_enumerate_next(&iter));

    list.count = ide_if_count - start_at;

    return list;
}

void ide_if_t::cleanup()
{
}

if_list_t ide_if_t::detect_devices()
{
    if_list_t list = {0, 0, 0};
    return list;
}
