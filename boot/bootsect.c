__asm__ (
    ".section .head, \"ax\"\n"
    ".globl halt\n"
    ".globl entry\n"
"entry:\n"
    // Set cs to 0000
    "ljmp $0,$entry_start\n"

    //
    // CDROM boot requires this space to be allocated in the boot sector
    // The CD mastering tool populates it with information
    //
    ".org entry+8\n\t"
    ".globl bootinfo_primary_volume_desc\n\t"
    "bootinfo_primary_volume_desc:"
    ".space 4\n\t"

    ".globl bootinfo_file_location\n\t"
    "bootinfo_file_location:"
    ".space 4\n\t"

    ".globl bootinfo_file_length\n\t"
    "bootinfo_file_length:"
    ".space 4\n\t"

    ".globl bootinfo_checksum\n\t"
    "bootinfo_checksum:"
    ".space 4\n\t"

    ".globl bootinfo_reserved\n\t"
    "bootinfo_reserved:"
    ".space 10*4\n\t"

    // The kernel finds the boot code by reading this vector
    // MP entry point vector at offset 64
    ".globl mp_entry_vector\n"
    "mp_entry_vector:\n\t"
    ".int 0\n\t"

    // The kernel finds the boot device information by reading this vector
    // Int 13h, AH=48h Get drive parameters
    // Vector at offset 68
    ".globl boot_device_info_vector\n\t"
    "boot_device_info_vector:\n\t"
    ".int 0\n\t"

    // The kernel finds the VBE information by reading this vector
    // Vector at offset 72
    ".globl vbe_info_vector\n\t"
    "vbe_info_vector:\n\t"
    ".int 0\n\t"

    // The bootstrap code starts here
    ".globl entry_start\n\t"
    "entry_start:\n\t"

    "0:\n\t"
    // Set ds and es to 0000
    "xorw %ax,%ax\n"
    "movw %ax,%ds\n"
    "movw %ax,%es\n"
    // Set initial stack pointer
    "pushw %ax\n"
    "pushl $__initial_stack\n"
    "movw %sp,%bx\n"
    "lss (%bx),%esp\n"
    // Clear bss
    "movl $___bss_en,%ecx\n"
    "movl $___bss_st,%edi\n"
    "subw %di,%cx\n"
    "cld\n"
    "rep stosb\n"

    // Store boot drive number
    "movb %dl,boot_drive\n"

    // Reset FPU
    "fninit\n"

    // Call C
    "call init\n"

    // Skip halt message
    "jmp 0f\n"
"halt:\n"
    // Skip print if not fully loaded
    "cmpb $0,fully_loaded\n"
    "je 0f\n"
    "pushl 4(%esp)\n"
    "call print_line\n"
"0:\n"
    "hlt\n"
    "jmp 0b\n"
    ".section .text, \"ax\"\n"
);

#include "types.h"

// If this is nonzero, then we are booting from CD
extern uint32_t bootinfo_file_location;
extern uint32_t bootinfo_primary_volume_desc;

#include "bootsect.h"
#include "part.h"
#include "fat32.h"
#include "iso9660.h"
#include "driveinfo.h"

#define __stdcall __attribute__((stdcall))
#define __packed __attribute__((packed))

uint8_t boot_drive __attribute__((used));
uint8_t fully_loaded __attribute__((used));

int init(void);

typedef struct disk_address_packet_t {
    uint8_t sizeof_packet;
    uint8_t reserved;
    uint16_t block_count;
    uint32_t address;
    uint64_t lba;
} disk_address_packet_t;

uint16_t read_lba_sectors(
        char *buf, uint8_t drive,
        uint32_t lba, uint16_t count)
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
    uint16_t ax = 0x4200;
    __asm__ __volatile__ (
        "int $0x13\n\t"
        "setc %%al\n\t"
        "neg %%al\n"
        "and %%al,%%ah\n"
        "shr $8,%%ax\n"
        : "+a" (ax)
        : "d" (drive),
          "S" (&pkt)
        : "memory"
    );
    return ax;
}

extern char __initialized_data_start[];
extern char __initialized_data_end[];

__attribute__((used)) int init(void)
{
    if (bootinfo_file_location == 0) {
        // Calculate how many more sectors to load
        uint16_t load_size = (__initialized_data_end - __initialized_data_start) >> 9;

        uint16_t err = read_lba_sectors((char*)(0x7c00u + 512),
                                       boot_drive, 1,
                                       load_size - 1);

        if (err != 0)
            halt(0);
    }

    fully_loaded = 1;

    driveinfo();

    if (bootinfo_file_location == 0)
        fat32_boot_partition(partition_table[0].start_lba);
    else
        iso9660_boot_partition(bootinfo_primary_volume_desc);

    return 0;
}
