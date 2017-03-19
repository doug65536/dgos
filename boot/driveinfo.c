#include "driveinfo.h"
#include "bootsect.h"
#include "malloc.h"
#include "farptr.h"

void driveinfo(void)
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

    uint16_t ax = 0x4800;
    __asm__ __volatile__ (
        "push %%ds\n\t"
        "mov %%si,%%ds\n\t"
        "xor %%si,%%si\n\t"
        "int $0x13\n\t"
        "setc %%al\n\t"
        "neg %%al\n\t"
        "and %%al,%%ah\n\t"
        "shr $16,%%ax\n\t"
        "pop %%ds\n\t"
        : "+a" (ax)
        : "d" (boot_drive)
        , "S" (ptr.segment)
        : "memory"
    );

    // Store the pointer so kernel can find it
    if (ax == 0)
        boot_device_info_vector = ptr.segment << 4;
}
