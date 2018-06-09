#include "malloc.h"
#include "screen.h"
#include "string.h"
#include "bootsect.h"
#include "rand.h"
#include "farptr.h"
#include "likely.h"

#define MALLOC_DEBUG 0
#if MALLOC_DEBUG
#define MALLOC_TRACE(...) PRINT("malloc: " __VA_ARGS__)
#else
#define MALLOC_TRACE(...) ((void)0)
#endif

struct blk_hdr_t {
    // Size including this header, in bytes
    uint32_t size;

    enum sig_t : uint32_t {
        FREE = 0xFEEEB10C,
        USED = 0xA10CA1ED
    };

    // Signature
    sig_t sig;

    uint32_t neg_size;
    blk_hdr_t *self;
};

static blk_hdr_t *heap_st;
static blk_hdr_t *first_free;

void malloc_init(void *st, void *en)
{
    heap_st = (blk_hdr_t*)((uintptr_t(st) + 15) & -16);

    // Initialize end of heap sentinel, just before end of heap
    blk_hdr_t *heap_end = (blk_hdr_t*)en - 1;
    heap_end->size = 0;
    heap_end->sig = blk_hdr_t::USED;
    heap_end->neg_size = -heap_end->size;
    heap_end->self = heap_end;

    // Make a free block covering the entire heap
    first_free = heap_st;
    heap_st->size = uintptr_t(heap_end) - uintptr_t(heap_st);
    heap_st->sig = blk_hdr_t::FREE;
    heap_st->neg_size = -heap_st->size;
    heap_st->self = heap_st;
}

#ifndef __efi
static __pure uintptr_t get_top_of_low_memory() {
    // Read top of memory from BIOS data area
    return *(uint16_t*)0x40E << 4;
}

extern "C" blk_hdr_t __heap_st[];
__attribute__((__constructor__)) void malloc_init_auto()
{
    // Memory map
    //
    // +-------------------------+ <- Start of BIOS data area (usually 0x9FC00)
    // |  blk_hdr_t with 0 size  |
    // +-------------------------+ <- 16 byte aligned
    // |         blocks          |
    // +-------------------------+ <- __heap_st, 16 byte aligned
    // |         .bss            |
    // |         .data           |
    // |         .text           |
    // +-------------------------+ <- 0x1000

    malloc_init(__heap_st, (void*)get_top_of_low_memory());
}
#endif

static blk_hdr_t *next_blk(blk_hdr_t *blk)
{
    return (blk_hdr_t*)(uintptr_t(blk) + blk->size);
}

__noreturn void malloc_panic()
{
    HALT("Corrupt heap block header!");
}

static void malloc_invalidate(blk_hdr_t *blk)
{
    blk->self = nullptr;
    blk->size = 0xBAD11111;
    blk->neg_size = 0;
}

static blk_hdr_t *malloc_coalesce(blk_hdr_t *blk, blk_hdr_t *next)
{
    // Coalesce adjacent consecutive free blocks
    while (blk->sig == blk_hdr_t::FREE && next->sig == blk_hdr_t::FREE) {
        blk->size += next->size;
        blk->neg_size = -blk->size;

        malloc_invalidate(next);

        // Enforce that malloc_rover is always pointing to a block header
        if (first_free == next)
            first_free = blk;

        next = next_blk(blk);
    }

    return next;
}

void __aligned(16) *malloc(size_t bytes)
{
    return malloc_aligned(bytes, 16);
}

void *malloc_aligned(size_t bytes, size_t alignment)
{
    if (unlikely(bytes == 0))
        return nullptr;

    // Remember starting point so we know when we have searched whole heap
    blk_hdr_t *blk = first_free;
    blk_hdr_t * const start_pos = blk;

    // Round up to a multiple of 16 bytes, increase by size of block header
    bytes = ((bytes + 15) & -16) + sizeof(blk_hdr_t);

    if (unlikely(blk->size + blk->neg_size || blk->self != blk))
        malloc_panic();

    do {
        blk_hdr_t *next = next_blk(blk);

        if (unlikely(next->size + next->neg_size || next->self != next))
            malloc_panic();

        next = malloc_coalesce(blk, next);

        if (blk->sig == blk_hdr_t::FREE) {
            if (first_free > blk || first_free->sig == blk_hdr_t::USED)
                first_free = blk;

            // Calculate how much more we would need to align the payload
            size_t align_adj =
                    ((uintptr_t(blk + 1) + alignment - 1) & -alignment) -
                    uintptr_t(blk + 1);

            if (blk->size >= bytes + align_adj) {
                // Found a sufficiently large free block

                if (align_adj) {
                    // Split the free block into two, and position the second
                    // one so its payload is at the alignment boundary
                    blk_hdr_t *aligned_hdr =
                            (blk_hdr_t*)(uintptr_t(blk) + align_adj);
                    aligned_hdr->size = blk->size - align_adj;
                    aligned_hdr->neg_size = -aligned_hdr->size;
                    aligned_hdr->self = aligned_hdr;
                    aligned_hdr->sig = blk_hdr_t::FREE;

                    if (first_free > aligned_hdr)
                        first_free = aligned_hdr;

                    blk->size = align_adj;
                    blk->neg_size = -blk->size;
                    blk = aligned_hdr;
                }

                size_t remain = blk->size - bytes;

                if (remain) {
                    // Take some of the block
                    // Make new block with unused portion
                    // It might be so small that there is a zero sized
                    // payload area, but it will coalesce eventually
                    next = (blk_hdr_t*)(uintptr_t(blk) + bytes);

                    next->size = remain;
                    next->neg_size = -next->size;
                    next->sig = blk_hdr_t::FREE;
                    next->self = next;
                }

                blk->size = bytes;
                blk->neg_size = -blk->size;
                blk->sig = blk_hdr_t::USED;

                return blk + 1;
            }
        }

        if (blk->size > 0) {
            blk = next;
        } else {
            // Reached the end of the heap, wrap around
            blk = heap_st;
        }
    } while (blk != start_pos);

    return nullptr;
}

void *realloc(void *p, size_t bytes)
{
    return realloc_aligned(p, bytes, 16);
}

// Note, alignment is only used when forced to allocate a new block
// Expanding or shrinking a block in-place ignores alignment parameter
void *realloc_aligned(void *p, size_t bytes, size_t alignment)
{
    if (unlikely(!p))
        return malloc_aligned(bytes, alignment);

    blk_hdr_t *blk = (blk_hdr_t*)p - 1;

    if (unlikely(blk->size + blk->neg_size || blk->self != blk))
        malloc_panic();

    bytes = ((bytes + 15) & -16) + sizeof(blk_hdr_t);

    blk_hdr_t *next = next_blk(blk);

    if (blk->size < bytes) {
        // Try to expand block in-place

        if (unlikely(next->size + next->neg_size || next->self != blk))
            malloc_panic();

        // If we are expanding and the next block is free
        // then coalesce free blocks after the next block
        if (blk->size < bytes && next->sig == blk_hdr_t::FREE)
            next = malloc_coalesce(next, next_blk(next));

        if (next->sig == blk_hdr_t::FREE && blk->size + next->size >= bytes) {
            // Expand in place

            blk_hdr_t *new_blk = (blk_hdr_t*)(uintptr_t(blk) + bytes);

            new_blk->size = uintptr_t(next) - uintptr_t(new_blk);
            new_blk->neg_size = -new_blk->size;
            new_blk->sig = blk_hdr_t::FREE;
            new_blk->self = new_blk;

            blk->size = uintptr_t(new_blk) - uintptr_t(blk);
            blk->neg_size = -blk->size;

            malloc_invalidate(next);

            return blk + 1;
        }

        // Unable to expand in place
        void *other_blk = malloc_aligned(bytes, alignment);
        if (!other_blk)
            return nullptr;

        memcpy(other_blk, blk + 1, blk->size - sizeof(*blk));

        blk->sig = blk_hdr_t::FREE;
        if (first_free > blk)
            first_free = blk;

        return other_blk;
    }

    if (blk->size > bytes) {
        // Shrink the block
        blk_hdr_t *new_blk = (blk_hdr_t*)(uintptr_t(blk) + bytes);

        new_blk->size = uintptr_t(next) - uintptr_t(new_blk);
        new_blk->neg_size = -new_blk->size;
        new_blk->self = new_blk;
        new_blk->sig = blk_hdr_t::FREE;

        blk->size = uintptr_t(new_blk) - uintptr_t(blk);
        blk->neg_size = -blk->size;

        return blk + 1;
    }

    // Block size did not change, do nothing
    return blk + 1;
}

void free(void *p)
{
    if (unlikely(!p))
        return;

    blk_hdr_t *blk = (blk_hdr_t*)p - 1;

    if (unlikely(blk->sig != blk_hdr_t::USED))
        HALT("Bad free call, block signature is not USED");

    if (unlikely(blk->size + blk->neg_size || blk->self != blk))
        malloc_panic();

    blk->sig = blk_hdr_t::FREE;

    if (first_free > blk)
        first_free = blk;
}

void *calloc(unsigned num, unsigned size)
{
    uint16_t bytes = num * size;
    void *block = malloc(bytes);
    memset(block, 0, bytes);
    return block;
}

void *operator new(size_t size) noexcept
{
    return malloc(size);
}

__const
void *operator new(size_t, void *p) noexcept
{
    return p;
}

void *operator new[](size_t size) noexcept
{
    return malloc(size);
}

void operator delete(void *block, unsigned long size) noexcept
{
    (void)size;
    free(block);
}

void operator delete(void *block, unsigned size) noexcept
{
    (void)size;
    free(block);
}

void operator delete(void *block) noexcept
{
    free(block);
}

void operator delete[](void *block) noexcept
{
    free(block);
}

void operator delete[](void *block, unsigned int) noexcept
{
    free(block);
}
