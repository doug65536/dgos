#include "types.h"

#include "bootsect.h"
#include "part.h"
#include "fat32.h"
#include "iso9660.h"
#include "driveinfo.h"
#include "screen.h"

uint8_t boot_drive __used;
uint8_t fully_loaded __used;

extern "C" int init();

struct disk_address_packet_t {
    uint8_t sizeof_packet;
    uint8_t reserved;
    uint16_t block_count;
    uint32_t address;
    uint64_t lba;
};

uint16_t read_lba_sectors(
        char *buf, uint8_t drive,
        uint32_t lba, uint16_t count)
{
    //if (fully_loaded)
    //    print_lba(lba);

    // Extended Read LBA sectors
    // INT 13h AH=42h
    disk_address_packet_t pkt = {
        sizeof(disk_address_packet_t),
        0,
        count,
        (uint32_t)buf,
        lba
    };
    uint16_t ax = 0x4200;
    __asm__ __volatile__ (
        "int $0x13\n\t"
        "setc %%al\n\t"
        "neg %%al\n"
        "and %%al,%%ah\n"
        "shr $8,%%ax\n"
        : "+a" (ax)
        : "d" (drive)
        , "S" (&pkt)
        : "memory"
    );
    return ax;
}

extern char __initialized_data_start[];
extern char __initialized_data_end[];
