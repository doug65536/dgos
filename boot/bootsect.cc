#include "types.h"
#include "bioscall.h"

extern uint8_t boot_drive;

uint8_t fully_loaded __used;

extern "C" int init();

struct disk_address_packet_t {
    uint8_t sizeof_packet;
    uint8_t reserved;
    uint16_t block_count;
    uint32_t address;
    uint64_t lba;
};

bool read_lba_sectors(char *buf, uint64_t lba, uint16_t count)
{
    // Extended Read LBA sectors
    // INT 13h AH=42h
    disk_address_packet_t pkt = {
        sizeof(disk_address_packet_t),
        0,
        count,
        (uint32_t)buf,
        lba
    };

    bios_regs_t regs;
    regs.eax = 0x4200;
    regs.edx = boot_drive;
    regs.esi = (uint32_t)&pkt;

    bioscall(&regs, 0x13);

    return regs.ah_if_carry() == 0;
}

extern char __initialized_data_start[];
extern char __initialized_data_end[];
