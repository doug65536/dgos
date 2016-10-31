#include "code16gcc.h"
asm (
    ".section .head\n"
    ".globl halt\n"
    ".globl entry\n"
"entry:\n"
    // Set cs to 0000
    "ljmp $0,$0f\n"
    "0:\n"
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
uint8_t boot_drive;
uint8_t fully_loaded;

int init(void);

//static void bochs_out(const char *msg)
//{
//    uint16_t ax = 0;
//    uint16_t cx = 0;
//    uint16_t dx = 0xE9;
//    uint16_t si = (uint16_t)(uint32_t)msg;
//    uint16_t di = (uint16_t)(uint32_t)msg;
//    __asm__ __volatile__ (
//        "decw %%cx\n\t"
//        "repnz scasb\n\t"
//
//        "notw %%cx\n\t"
//        "decw %%cx\n\t"
//        "rep outsb\n\t"
//
//        "movb $0x0a,%%al\n\t"
//        "outb %%al,$0xe9\n\t"
//        : "=a" (ax), "=c" (cx), "=S" (si), "=D" (di)
//        : "a" (ax), "c" (cx), "d" (dx), "S" (si), "D" (di)
//        : "memory"
//    );
//}
//
//void bochs_int(uint32_t n)
//{
//    char buf[12], *p = buf + 12;
//    *--p = 0;
//    while (n != 0)
//    {
//        *--p = '0' + (n%10);
//        n /= 10;
//    }
//    bochs_out(p);
//}

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
        "movzbw %h0,%w0\n\t"
        : "=a" (ax)
        : "a" (ax), "b" (buf), "c" (cx), "d" (dx)
        : "memory"
    );
    // AX contains BIOS error code, or 0 on success

    return ax;
}

uint16_t read_lba_sector(char *buf, uint8_t drive, uint32_t lba)
{
    return read_chs_sector(buf, drive, lba_to_chs(lba));
}

extern char __initialized_data_start[];
extern char __initialized_data_end[];

int init(void)
{
    // Prepare to do LBA to CHS conversions
    drive_geometry = get_drive_geometry(boot_drive);

    // Calculate how many more sectors to load
    uint16_t load_size = (__initialized_data_end - __initialized_data_start) >> 9;

    // Load more sectors
    for (uint32_t i = 1; i < load_size; ++i) {
        uint16_t err = read_lba_sector((char*)(0x7c00u + (i << 9)), boot_drive, i);

        if (err != 0)
            halt(0);
    }

    fully_loaded = 1;

    boot_partition(partition_table[0].start_lba);

    return 0;
}
