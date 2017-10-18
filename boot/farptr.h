#pragma once

#include "types.h"
#include "string.h"

struct far_ptr_t {
    uint16_t offset;
    uint16_t segment;
};

static inline void *far_to_addr(far_ptr_t const& ptr)
{
    return (void*)((ptr.segment << 4) + ptr.offset);
}

static inline void far_zero(far_ptr_t dest, uint16_t paragraphs)
{
    void *addr = far_to_addr(dest);
    size_t size = paragraphs << 4;
    memset(addr, 0, size);
}

static inline void far_copy(far_ptr_t dest,
              far_ptr_t src,
              uint16_t size)
{
    void *dest_addr = far_to_addr(dest);
    void *src_addr = far_to_addr(src);
    memcpy(dest_addr, src_addr, size);
}

static inline far_ptr_t far_ptr2(uint16_t seg, uint16_t ofs)
{
    far_ptr_t ptr;
    ptr.offset = ofs;
    ptr.segment = seg;
    return ptr;
}

static inline far_ptr_t far_ptr(uint32_t addr)
{
    return far_ptr2(addr >> 4, addr & 0x0F);
}

static inline void far_copy_to(void *near, far_ptr_t src, uint16_t size)
{
	far_copy(far_ptr(uint32_t(near)), src, size);
}

static inline void far_copy_from(far_ptr_t dest, void *near, uint16_t size)
{
	far_copy(dest, far_ptr(uint32_t(near)), size);
}

// Adjust a far pointer and renormalize the segment to
// have the smallest possible offset
static inline far_ptr_t far_adj(far_ptr_t ptr, int32_t distance)
{
    uint32_t addr = ((uint32_t)ptr.segment << 4) + ptr.offset;
    addr += distance;
    return far_ptr2(addr >> 4, addr & 0x0F);
}
