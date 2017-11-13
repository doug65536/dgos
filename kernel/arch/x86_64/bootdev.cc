#include "bootdev.h"
#include "printk.h"
#include "bootinfo.h"

struct bootdev_drive_params_t {
    uint16_t size;
    uint16_t info_flags;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint64_t total_sectors;
    uint16_t sector_size;
    uint16_t edd_ofs;
    uint16_t edd_seg;
    uint16_t dev_path_sig;
    uint8_t dev_path_len;
    uint8_t reserved[3];
    uint8_t host_bus[4];
    uint8_t interface_type[8];

    union {
        struct {
            uint16_t base_addr;
            uint8_t unused[6];
        } isa;
        struct {
            uint8_t bus;
            uint8_t dev;
            uint8_t func;
            uint8_t unused[5];
        } pci;
    } interface_path;

    union {
        struct {
            uint8_t slave;
            uint8_t unused[7];
        } ata;

        struct {
            uint8_t slave;
            uint8_t lun;
            uint8_t unused[6];
        } atapi;

        struct {
            uint8_t lun;
            uint8_t unused[7];
        } scsi;

        struct {
            uint8_t tbd;
            uint8_t unused[7];
        } usb;

        struct {
            uint64_t guid;
        } ieee1394;

        struct {
            uint8_t wwn;
        } fibrechannel;
    } device_path;

    uint8_t reserved2;
    uint8_t checksum;
} __packed;

void bootdev_info(uint8_t *bus, uint8_t *slot, uint8_t *func)
{
    (void)bus;
    (void)slot;
    (void)func;

    bootdev_drive_params_t *p = (bootdev_drive_params_t*)
//            (uintptr_t)*(uint32_t*)0x7C44;
            bootinfo_parameter(bootparam_t::boot_device);

    printdbg("Host bus %4.4s %8.8s\n", p->host_bus, p->interface_type);
}
