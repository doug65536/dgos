#pragma once

#include "types.h"
#include "string.h"

struct far_ptr_t {
    uint16_t offset;
    uint16_t segment;
};

static __always_inline uintptr_t seg_to_addr(uint16_t seg)
{
    return (seg << 4);
}

static __always_inline void *seg_to_ptr(uint16_t seg)
{
    return (void*)seg_to_addr(seg);
}

static __always_inline uintptr_t far_to_addr(far_ptr_t const& ptr)
{
    return seg_to_addr(ptr.segment) + ptr.offset;
}

static __always_inline void *far_to_ptr(far_ptr_t const& ptr)
{
    return (void*)far_to_addr(ptr);
}

static __always_inline void far_zero(far_ptr_t dest, unsigned paragraphs)
{
    void *addr = far_to_ptr(dest);
    size_t size = paragraphs << 4;
    memset(addr, 0, size);
}

static __always_inline void far_copy(far_ptr_t dest,
              far_ptr_t src, size_t size)
{
    void *dest_addr = far_to_ptr(dest);
    void *src_addr = far_to_ptr(src);
    memcpy(dest_addr, src_addr, size);
}

static __always_inline far_ptr_t far_ptr2(uint16_t seg, uint16_t ofs)
{
    far_ptr_t ptr;
    ptr.offset = ofs;
    ptr.segment = seg;
    return ptr;
}

static __always_inline far_ptr_t far_ptr(uint32_t addr)
{
    return far_ptr2(uint16_t(addr >> 4), uint16_t(addr & 0x0F));
}

static __always_inline void far_copy_to(
        void *near, far_ptr_t src, uint16_t size)
{
    far_copy(far_ptr(uintptr_t(near)), src, size);
}

static __always_inline void far_copy_from(
        far_ptr_t dest, void *near, uint16_t size)
{
    far_copy(dest, far_ptr(uintptr_t(near)), size);
}

// Adjust a far pointer and renormalize the segment to
// have the smallest possible offset
static __always_inline far_ptr_t far_adj(far_ptr_t ptr, int32_t distance)
{
    uint32_t addr = ((uint32_t)ptr.segment << 4) + ptr.offset;
    addr += distance;
    return far_ptr2(uint16_t(addr >> 4), uint16_t(addr & 0x0F));
}
