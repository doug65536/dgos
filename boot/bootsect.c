asm (
    ".section .head\n"
    ".globl halt\n"
    ".globl entry\n"
"entry:\n"
    // Set cs to 0000
    "ljmp $0,$entry_start\n"
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

    // MP entry point vector at offset 64
    ".globl mp_entry_vector\n"
    "mp_entry_vector:\n\t"
    ".int 0\n\t"

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
    //"pushl $0xE3F\n"
    //"fldcw (%esp)\n"
    //"addl $4,%esp\n"

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
    ".section .text\n"
);

#include "bootsect.h"
#include "part.h"
#include "fat32.h"

#define __stdcall __attribute__((stdcall))
#define __packed __attribute__((packed))

uint8_t boot_drive __attribute__((used));
uint8_t fully_loaded __attribute__((used));

int init(void);

// Using int13 extensions is 125 bytes less code
#define USE_INT13EXT 1
#if USE_INT13EXT
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
#else
typedef struct
{
    uint8_t sectors;
    uint8_t heads;
} drive_geometry_t;

typedef struct
{
    uint8_t sector;
    uint8_t head;
    uint16_t cylinder;
} chs_t;

drive_geometry_t drive_geometry;

// INT 0x13
// AH = 0x08
static drive_geometry_t get_drive_geometry(uint8_t drive)
{
    drive_geometry_t geometry;
    short dx, cx;
    // Set ES:DI to 0 to workaround possible BIOS bugs
    __asm__ __volatile__ (
        "int $0x13\n\t"
        : "=d" (dx), "=c" (cx)
        : "a" (0x800), "d" (drive), "D" (0)
    );
    geometry.sectors = cx & 0x3F;
    geometry.heads = (dx >> 8) + 1;
    return geometry;
}

static chs_t lba_to_chs(int lba)
{
    uint32_t temp = lba / drive_geometry.sectors;
    chs_t chs;
    chs.sector = (lba % drive_geometry.sectors) + 1;
    chs.head = temp % drive_geometry.heads;
    chs.cylinder = temp / drive_geometry.heads;
    return chs;
}

static uint16_t read_chs_sector(char *buf, uint8_t drive, chs_t chs)
{
    // http://www.ctyme.com/intr/rb-0607.htm
    // AH = 02h
    // AL = number of sectors to read (must be nonzero)
    // CH = low eight bits of cylinder number
    // CL = sector number 1-63 (bits 0-5)
    // high two bits of cylinder (bits 6-7, hard disk only)
    // DH = head number
    // DL = drive number (bit 7 set for hard disk)
    // ES:BX -> data buffer
    uint16_t ax =
            (2 << 8) |								// AX 15:8 = 2
            1;										// AX 7:0 = sector count
    uint16_t cx =
            ((chs.cylinder & 0xFF) << 8) |			// CX 15:8 = cylinder 7:0
            ((chs.cylinder >> 2) & 0xC0) |			// CX 7:6 = cylinder 9:8
            chs.sector;								// CX 5:0 = sector
    uint16_t dx =
            ((uint16_t)chs.head << 8) |				// DX 15:8 = head
            drive;									// DX 7:0 = drive

    __asm__ __volatile__ (
        "int $0x13\n\t"
        // set AL to 1 if carry is set
        "setc %b0\n\t"
        // set AL to 0xFF if AL is 1, no effect if AL is 0
        "negb %b0\n\t"
        // clear AH if carry was not set
        "andb %b0,%h0\n\t"
        // zero extend AH into AX
        "shr $8,%%ax\n\t"
        : "=a" (ax)
        : "a" (ax), "b" (buf), "c" (cx), "d" (dx)
        : "memory"
    );
    // AX contains BIOS error code, or 0 on success

    return ax;
}

uint16_t read_lba_sectors(char *buf, uint8_t drive,
                          uint32_t lba, uint16_t count)
{
    uint16_t err;
    for (int off = 0; off < count; ++off,
         buf += (1<<9), ++lba) {
        if (0 != (err = read_chs_sector(
                      buf,
                      drive, lba_to_chs(lba))))
            return err;
    }
    return 0;
}
#endif

extern char __initialized_data_start[];
extern char __initialized_data_end[];

__attribute__((used)) int init(void)
{
#if !USE_INT13EXT
    // Prepare to do LBA to CHS conversions
    drive_geometry = get_drive_geometry(boot_drive);
#endif

    // Calculate how many more sectors to load
    uint16_t load_size = (__initialized_data_end - __initialized_data_start) >> 9;

    uint16_t err = read_lba_sectors((char*)(0x7c00u + 512),
                                   boot_drive, 1,
                                   load_size - 1);

    if (err != 0)
        halt(0);

    fully_loaded = 1;

    boot_partition(partition_table[0].start_lba);

    return 0;
}
