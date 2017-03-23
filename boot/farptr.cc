#include "farptr.h"

void far_zero(far_ptr_t dest, uint16_t paragraphs)
{
    uint32_t eax = 0;
    __asm__ __volatile__ (
        "push %%ds\n\t"
        "0:\n\t"
        "movw %w[seg],%%ds\n\t"
        "movl %[zero],(%[ofs])\n\t"
        "movl %[zero],4(%[ofs])\n\t"
        "movl %[zero],8(%[ofs])\n\t"
        "movl %[zero],12(%[ofs])\n\t"
        "decw %%es:%w[paragraphs]\n\t"
        "jnz 0b\n\t"
        "pop %%ds\n\t"
        : [paragraphs] "+m" (paragraphs)
        : [zero] "a" (eax)
        , [ofs] "D" (dest.offset)
        , [seg] "c" (dest.segment)
        : "memory"
    );
}

void far_copy_to(void *near, far_ptr_t src, uint16_t size)
{
    far_copy(far_ptr((uint32_t)near), src, size);
}

void far_copy_from(far_ptr_t dest, void *near, uint16_t size)
{
    far_copy(dest, far_ptr((uint32_t)near), size);
}

void far_copy(far_ptr_t dest,
              far_ptr_t src,
              uint16_t size)
{
    __asm__ __volatile__ (
        "pushw %%ds\n\t"
        "pushw %%si\n\t"
        "pushw %%es\n\t"
        "pushw %%di\n\t"

        "les %[dest],%%di\n\t"
        "lds %[src],%%si\n\t"
        "cld\n\t"
        "rep movsb\n\t"

        "popw %%di\n\t"
        "popw %%es\n\t"
        "popw %%si\n\t"
        "popw %%ds\n\t"
        : "+c" (size)
        : [dest] "m" (dest), [src] "m" (src)
        : "memory"
    );
}

// Adjust a far pointer and renormalize the segment to
// have the smallest possible offset
far_ptr_t far_adj(far_ptr_t ptr, int32_t distance)
{
    uint32_t addr = ((uint32_t)ptr.segment << 4) + ptr.offset;
    addr += distance;
    return far_ptr2(addr >> 4, addr & 0x0F);
}

far_ptr_t far_ptr2(uint16_t seg, uint16_t ofs)
{
    far_ptr_t ptr;
    ptr.offset = ofs;
    ptr.segment = seg;
    return ptr;
}

far_ptr_t far_ptr(uint32_t addr)
{
    return far_ptr2(addr >> 4, addr & 0x0F);
}
