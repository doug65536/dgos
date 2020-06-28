#include "malloc.h"
#include "screen.h"
#include "string.h"
#include "diskio.h"
#include "rand.h"
#include "farptr.h"
#include "likely.h"
#include "ctors.h"
#include "halt.h"

#define MALLOC_DEBUG 0
#if MALLOC_DEBUG
#define MALLOC_TRACE(...) PRINT("malloc: " __VA_ARGS__)
#else
#define MALLOC_TRACE(...) ((void)0)
#endif

#define MALLOC_CHECKS 1
#if MALLOC_CHECKS
#define MALLOC_CHECK() malloc_validate_or_panic()
#else
#define MALLOC_CHECK() ((void)0)
#endif

ext::nothrow_t const ext::nothrow;

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
    uint32_t self;

    _always_inline bool invalid() const
    {
        return size + neg_size ||
                uint32_t(intptr_t(self)) != uint32_t(intptr_t(this));
    }

    _always_inline void make_invalid()
    {
        self = uintptr_t(nullptr);
        size = 0xBAD11111;
        neg_size = 0;
    }

    _always_inline void make_valid()
    {
        // It doesn't matter if this is truncated, the LSBs are plenty
        self = uint32_t(intptr_t(this));
    }

    _always_inline void set_size(uint32_t new_size)
    {
        size = new_size;
        neg_size = -new_size;
    }
};

static_assert(sizeof(blk_hdr_t) == 16, "Unexpected malloc block header size");

static blk_hdr_t *heap_st, *heap_en;
static blk_hdr_t *first_free;

void malloc_init(void *st, void *en)
{
    // Align boundaries
    st = (void*)((uintptr_t(st) + 15) & -16);
    en = (void*)(uintptr_t(en) & -16);
    heap_st = (blk_hdr_t*)st;

    // Initialize end of heap sentinel, just before end of heap
    heap_en = (blk_hdr_t*)en - 1;
    heap_en->set_size(0);
    heap_en->make_valid();
    heap_en->sig = blk_hdr_t::USED;

    // Make a free block covering the entire heap
    first_free = heap_st;
    heap_st->set_size(uintptr_t(heap_en) - uintptr_t(heap_st));
    heap_st->make_valid();
    heap_st->sig = blk_hdr_t::FREE;
}

static blk_hdr_t *next_blk(blk_hdr_t *blk)
{
    return (blk_hdr_t*)(uintptr_t(blk) + blk->size);
}

_noreturn void malloc_panic()
{
    PANIC("Corrupt heap block header!");
}

static blk_hdr_t *malloc_coalesce(blk_hdr_t *blk, blk_hdr_t *next)
{
    // Coalesce adjacent consecutive free blocks
    while (unlikely(blk->sig == blk_hdr_t::FREE &&
                    next->sig == blk_hdr_t::FREE)) {
        blk->set_size(blk->size + next->size);

        next->make_invalid();

        // Enforce that malloc rover is always pointing to a block header
        if (first_free == next)
            first_free = blk;

        next = next_blk(blk);
    }

    return next;
}

void _aligned(16) *malloc(size_t bytes)
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

    if (unlikely(blk->invalid()))
        malloc_panic();

    do {
        blk_hdr_t *next = next_blk(blk);

        if (unlikely(next->invalid()))
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
                    aligned_hdr->set_size(blk->size - align_adj);
                    aligned_hdr->make_valid();
                    aligned_hdr->sig = blk_hdr_t::FREE;

                    if (first_free > aligned_hdr)
                        first_free = aligned_hdr;

                    blk->set_size(align_adj);
                    blk = aligned_hdr;
                }

                size_t remain = blk->size - bytes;

                if (remain) {
                    // Take some of the block
                    // Make new block with unused portion
                    // It might be so small that there is a zero sized
                    // payload area, but it will coalesce eventually
                    next = (blk_hdr_t*)(uintptr_t(blk) + bytes);

                    next->set_size(remain);
                    next->make_valid();
                    next->sig = blk_hdr_t::FREE;
                }

                blk->set_size(bytes);
                blk->sig = blk_hdr_t::USED;

                MALLOC_CHECK();

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

    MALLOC_CHECK();

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

    if (unlikely(blk->invalid()))
        malloc_panic();

    bytes = ((bytes + 15) & -16) + sizeof(blk_hdr_t);

    blk_hdr_t *next = next_blk(blk);

    if (blk->size < bytes) {
        // Try to expand block in-place

        if (unlikely(next->invalid()))
            malloc_panic();

        // If we are expanding and the next block is free
        // then coalesce free blocks after the next block
        if (blk->size < bytes && next->sig == blk_hdr_t::FREE)
            next = malloc_coalesce(next, next_blk(next));

        if (next->sig == blk_hdr_t::FREE && blk->size + next->size >= bytes) {
            // Expand in place

            blk_hdr_t *new_blk = (blk_hdr_t*)(uintptr_t(blk) + bytes);

            new_blk->set_size(uintptr_t(next) - uintptr_t(new_blk));
            new_blk->make_valid();
            new_blk->sig = blk_hdr_t::FREE;

            blk->set_size(uintptr_t(new_blk) - uintptr_t(blk));

            next->make_invalid();

            return blk + 1;
        }

        // Unable to expand in place
        void *other_blk = malloc_aligned(bytes, alignment);
        if (unlikely(!other_blk))
            return nullptr;

        memcpy(other_blk, blk + 1, blk->size - sizeof(*blk));

        blk->sig = blk_hdr_t::FREE;
        if (first_free > blk)
            first_free = blk;

        return other_blk;
    }

    if (blk->size > bytes) {
        // Shrink the block

        // Make a new free block with deallocated region
        blk_hdr_t *new_blk = (blk_hdr_t*)(uintptr_t(blk) + bytes);

        new_blk->set_size(uintptr_t(next) - uintptr_t(new_blk));
        new_blk->make_valid();
        new_blk->sig = blk_hdr_t::FREE;

        blk->set_size(uintptr_t(new_blk) - uintptr_t(blk));

        MALLOC_CHECK();

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
        PANIC("Bad free call, block signature is not USED");

    if (unlikely(blk->invalid()))
        malloc_panic();

    blk->sig = blk_hdr_t::FREE;

    if (first_free > blk)
        first_free = blk;

    MALLOC_CHECK();
}

void *calloc(size_t num, size_t size)
{
    uint16_t bytes = num * size;
    void *block = malloc(bytes);
    memset(block, 0, bytes);

    MALLOC_CHECK();

    return block;
}

bool malloc_validate_or_panic()
{
    if (unlikely(!malloc_validate()))
        PANIC("Heap validation failed");
    return true;
}

bool malloc_validate()
{
    for (blk_hdr_t *blk = heap_st; ;
         blk = (blk_hdr_t*)(uintptr_t(blk) + blk->size)) {
        if (blk->invalid() ||
                (blk->sig != blk_hdr_t::FREE && blk->sig != blk_hdr_t::USED)) {
            PRINT("Invalid block header at %zx\n", uintptr_t(blk));
            return false;
        }

        if (blk->size & 15) {
            PRINT("Block size is not a multiple of 16\n");
            return false;
        }

        if (blk < heap_st || blk > heap_en) {
            PRINT("Went off the the heap into the weeds\n");
            return false;
        }

        if (blk == heap_en) {
            if (blk->size != 0) {
                PRINT("Heap end sentinel has invalid size\n");
                return false;
            }

            break;
        }
    }

    return true;
}

// deleted, must use nothrow
//void* operator new(size_t count, std::align_val_t alignment)
//{
//    return malloc_aligned(count, size_t(alignment));
//}

// deleted, must use nothrow
//void* operator new[](size_t count, std::align_val_t alignment)
//{
//    return malloc_aligned(count, size_t(alignment));
//}

void* operator new[](size_t count, std::align_val_t alignment,
    ext::nothrow_t const&) noexcept
{
    return malloc_aligned(count, size_t(alignment));
}

// deleted, must use nothrow
//void *operator new(size_t size)
//{
//    return malloc(size);
//}

void *operator new(size_t size, ext::nothrow_t const&) noexcept
{
    return malloc(size);
}

void *operator new[](size_t size, ext::nothrow_t const&) noexcept
{
    return malloc(size);
}

_const
void *operator new(size_t, void *p)
{
    return p;
}

//void *operator new[](size_t size)
//{
//    return malloc(size);
//}

void operator delete(void *block, unsigned long size)
{
    (void)size;
    free(block);
}

void operator delete(void *block, unsigned size)
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

void operator delete[](void *block, unsigned int)
{
    free(block);
}

void operator delete[](void *block, unsigned long)
{
    free(block);
}

void malloc_get_heap_range(void **st, void **en)
{
    *st = heap_st;
    *en = heap_en + 1;
}

char *strdup(char const *s)
{
    size_t len = strlen(s);
    char *copy = (char*)malloc(len + 1);
    if (likely(copy))
        return (char*)memcpy(copy, s, len + 1);
    return nullptr;
}
