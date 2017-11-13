#include "driveinfo.h"
#include "bootsect.h"
#include "malloc.h"
#include "farptr.h"
#include "bioscall.h"

uint16_t driveinfo()
{
    // AH = 48h
    // DL = drive (80h-FFh)
    // DS:SI -> buffer for drive parameters (see #00273)

    uint16_t size = 0x42;
    far_ptr_t ptr;
    ptr.segment = far_malloc(size);
    ptr.offset = 0;
    far_zero(ptr, size >> 4);

    // Write the size into the memory
    far_copy_from(ptr, &size, sizeof(size));

    bios_regs_t regs;
    regs.eax = 0x4800;
    regs.ds = ptr.segment;
    regs.edx = boot_drive;
    regs.esi = ptr.offset;

    bioscall(&regs, 0x13);

    // Store the pointer so kernel can find it
    if (!regs.flags_CF())
        boot_device_info_vector = ptr.segment << 4;

    return regs.ah_if_carry();
}
