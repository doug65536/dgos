#include "heap.h"
#include "threadsync.h"
#include "assert.h"
#include "string.h"
#include "bitsearch.h"

#ifdef __DGOS_KERNEL__
#include "mm.h"
#include "cpu/control_regs.h"
#else
#include <pthread.h>
#define mutex_init pthread_mutex_init
#define mutex_destroy pthread_mutex_destroy
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock
#endif

// Enable wiping freed memory with 0xfe
// and filling allocated memory with 0xf0
#define HEAP_DEBUG  1

// Always use paged allocation with guard pages
// Realloc always moves the memory to a new range
#define HEAP_PAGEONLY 0

// Don't free virtual address ranges, just free physical pages
#define HEAP_NOVFREE 1

struct heap_hdr_t {
    uintptr_t size_next;
    uint32_t sig1;
    uint32_t sig2;
};

C_ASSERT(sizeof(heap_hdr_t) == 16);

static constexpr uint32_t HEAP_BLK_TYPE_USED = 0xeda10ca1;  // "a10ca1ed"
static constexpr uint32_t HEAP_BLK_TYPE_FREE = 0x0cb1eefe;  // "feeeb10c"

#if !HEAP_PAGEONLY

/// bucket  slot sz item sz items efficiency
/// [ 0] ->      32      16  2048     50.00%
/// [ 1] ->      64      48  1024     75.00%
/// [ 2] ->     128     112   512     87.50%
/// [ 3] ->     256     240   256     93.75%
/// [ 4] ->     512     496   128     96.88%
/// [ 5] ->    1024    1008    64     98.44%
/// [ 6] ->    2048    2032    32     99.22%
/// [ 7] ->    4096    4080    16     99.61%
/// [ 8] ->    8192    8176     8     99.80%
/// [ 9] ->   16384   16368     4     99.90%
/// [10] ->   32768   32752     2     99.95%
/// [11] ->   65536   65520     1     99.98%
/// .... -> use mmap

static constexpr size_t HEAP_BUCKET_COUNT = 12;

static constexpr size_t HEAP_MMAP_THRESHOLD =
        (size_t(1)<<(5+HEAP_BUCKET_COUNT-1));

static constexpr size_t HEAP_BUCKET_SIZE =
        (size_t(1)<<(HEAP_BUCKET_COUNT+5-1));

// When the main heap_t::arenas array overflows, an additional
// page is allocated to hold additional arena pointers.
struct heap_ext_arena_t {
    heap_ext_arena_t *prev;
    size_t arena_count;
    void *arenas[(PAGESIZE - sizeof(heap_ext_arena_t*) -
                  sizeof(size_t)) / sizeof(void*)];
};

// The maximum number of arenas without adding extended arenas
static constexpr size_t HEAP_MAX_ARENAS =
    (((PAGESIZE -
    sizeof(void*) * HEAP_BUCKET_COUNT -
    sizeof(heap_ext_arena_t*) -
    sizeof(mutex_t)) /
    sizeof(void*)) - 1);

C_ASSERT(sizeof(heap_ext_arena_t) == PAGESIZE);

struct heap_t {
    heap_hdr_t *free_chains[HEAP_BUCKET_COUNT];
    mutex_t lock;

    // The first HEAP_MAX_ARENAS arena pointers are here
    void *arenas[HEAP_MAX_ARENAS];
    size_t arena_count;

    // Singly linked list of overflow arena pointer pages
    heap_ext_arena_t *last_ext_arena;
};

C_ASSERT(sizeof(heap_t) == PAGESIZE);

heap_t *heap_create(void)
{
    heap_t *heap = (heap_t*)mmap(0, sizeof(heap_t), PROT_READ | PROT_WRITE,
                                 MAP_UNINITIALIZED | MAP_POPULATE, -1, 0);
    if (unlikely(heap == MAP_FAILED))
        return nullptr;
    memset(heap->free_chains, 0, sizeof(heap->free_chains));
    memset(heap->arenas, 0, sizeof(heap->arenas));
    heap->arena_count = 0;
    heap->last_ext_arena = 0;
    mutex_init(&heap->lock);
    return heap;
}

void heap_destroy(heap_t *heap)
{
    mutex_lock(&heap->lock);

    // Free buckets in extended arena lists
    // and the extended arena lists themselves
    heap_ext_arena_t *ext_arena = heap->last_ext_arena;
    while (ext_arena) {
        for (size_t i = 0; i < ext_arena->arena_count; ++i)
            munmap(ext_arena->arenas[i], HEAP_BUCKET_SIZE);
        ext_arena = ext_arena->prev;
        munmap(ext_arena, PAGESIZE);
    }

    // Free the buckets in the main arena list
    for (size_t i = 0; i < heap->arena_count; ++i)
        munmap(heap->arenas[i], HEAP_BUCKET_SIZE);

    mutex_unlock(&heap->lock);
    mutex_destroy(&heap->lock);

    munmap(heap, sizeof(*heap));
}

static heap_hdr_t *heap_create_arena(heap_t *heap, uint8_t log2size)
{
    size_t *arena_count_ptr;
    void **arena_list_ptr;

    if (heap->arena_count < countof(heap->arenas)) {
        // Main arena list
        arena_count_ptr = &heap->arena_count;
        arena_list_ptr = heap->arenas;
    } else if (heap->last_ext_arena &&
               heap->last_ext_arena->arena_count <
               countof(heap->last_ext_arena->arenas)) {
        // Last overflow arena has room
        arena_count_ptr = &heap->last_ext_arena->arena_count;
        arena_list_ptr = heap->last_ext_arena->arenas;
    } else {
        // Create a new arena overflow page
        heap_ext_arena_t *new_list;

        new_list = (heap_ext_arena_t*)mmap(
                    0, PAGESIZE, PROT_READ | PROT_WRITE,
                    MAP_UNINITIALIZED, -1, 0);
        if (!new_list || new_list == MAP_FAILED)
            return 0;

        new_list->prev = heap->last_ext_arena;
        new_list->arena_count = 0;
        heap->last_ext_arena = new_list;

        arena_count_ptr = &new_list->arena_count;
        arena_list_ptr = new_list->arenas;
    }

    size_t bucket = log2size - 5;

    char *arena = (char*)mmap(0, HEAP_BUCKET_SIZE, PROT_READ | PROT_WRITE,
                       MAP_POPULATE | MAP_UNINITIALIZED, -1, 0);
    char *arena_end = arena + HEAP_BUCKET_SIZE;

    arena_list_ptr[(*arena_count_ptr)++] = arena;

    size_t size = 1U << log2size;

    heap_hdr_t *hdr = 0;
    heap_hdr_t *first_free = heap->free_chains[bucket];
    for (char *fill = arena_end - size; fill >= arena; fill -= size) {
        hdr = (heap_hdr_t*)fill;
        hdr->size_next = uintptr_t(first_free);
        hdr->sig1 = HEAP_BLK_TYPE_FREE;
        first_free = hdr;
    }
    heap->free_chains[bucket] = first_free;

    return hdr;
}

void *heap_calloc(heap_t *heap, size_t num, size_t size)
{
    size *= num;
    void *block = heap_alloc(heap, size);

    return memset(block, 0, size);
}

static void *heap_large_alloc(size_t size)
{
    heap_hdr_t *hdr = (heap_hdr_t*)mmap(0, size,
                           PROT_READ | PROT_WRITE,
                           MAP_UNINITIALIZED, -1, 0);

    assert(hdr != MAP_FAILED);

    if (hdr == MAP_FAILED)
        return nullptr;

    hdr->size_next = size;
    hdr->sig1 = HEAP_BLK_TYPE_USED;
    hdr->sig2 = HEAP_BLK_TYPE_USED ^ uint32_t(size);

    return hdr + 1;
}

static void heap_large_free(heap_hdr_t *hdr, size_t size)
{
    munmap(hdr, size);
}

void *heap_alloc(heap_t *heap, size_t size)
{
    if (unlikely(size == 0))
        return nullptr;

    // Add room for bucket header
    size += sizeof(heap_hdr_t);

    size_t orig_size = size;

    // Calculate ceil(log(size) / log(2))
    uint8_t log2size = bit_log2(size);

    // Round up to bucket item size
    size = size_t(1) << log2size;

    // Bucket 0 is 32 bytes
    assert(log2size >= 5);
    size_t bucket = log2size - 5;

    heap_hdr_t *first_free;

    if (unlikely(bucket >= HEAP_BUCKET_COUNT))
        return heap_large_alloc(orig_size);

    {
        // Disable irqs to allow malloc in interrupt handlers
#ifdef __DGOS_KERNEL__
        cpu_scoped_irq_disable intr_was_enabled;
#endif

        mutex_lock(&heap->lock);

        // Try to take a free item
        first_free = heap->free_chains[bucket];

        if (!first_free) {
            // Create a new bucket
            first_free = heap_create_arena(heap, log2size);
        }

        // Remove block from chain
        heap->free_chains[bucket] = (heap_hdr_t*)first_free->size_next;

        mutex_unlock(&heap->lock);
    }

    if (likely(first_free)) {
        // Store size (including size of header) in header
        first_free->size_next = size;

        assert(first_free->sig1 == HEAP_BLK_TYPE_FREE);

        first_free->sig1 = HEAP_BLK_TYPE_USED;
        first_free->sig2 = HEAP_BLK_TYPE_USED ^ uint32_t(size);

#if HEAP_DEBUG
        memset(first_free + 1, 0xf0, size - sizeof(*first_free));
#endif

        return first_free + 1;
    }

    return 0;
}

void heap_free(heap_t *heap, void *block)
{
    if (unlikely(!block))
        return;

    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

    assert(hdr->sig1 == HEAP_BLK_TYPE_USED);

#if HEAP_DEBUG
    memset(block, 0xfe, hdr->size_next - sizeof(*hdr));
#endif

    uint8_t log2size = bit_log2(hdr->size_next);
    assert(log2size >= 5 && log2size < 32);
    size_t bucket = log2size - 5;

    if (bucket < HEAP_BUCKET_COUNT) {
        assert(hdr->sig2 == (HEAP_BLK_TYPE_USED ^ (size_t(1) << log2size)));

        hdr->sig1 = HEAP_BLK_TYPE_FREE;

        cpu_scoped_irq_disable intr_was_enabled;
        mutex_lock(&heap->lock);
        hdr->size_next = uintptr_t(heap->free_chains[bucket]);
        heap->free_chains[bucket] = hdr;
        mutex_unlock(&heap->lock);
    } else {
        heap_large_free(hdr, hdr->size_next);
    }
}

void *heap_realloc(heap_t *heap, void *block, size_t size)
{
    if (block && size) {
        heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

        assert(hdr->sig1 == HEAP_BLK_TYPE_USED);

        uint8_t log2size = bit_log2(hdr->size_next);
        uint8_t newlog2size = bit_log2(size + sizeof(heap_hdr_t));

        // Reallocating to a size that is in the same bucket is a no-op
        // If it ends up in a different bucket...
        if (newlog2size != log2size) {
            // Allocate a new block
            void *new_block = heap_alloc(heap, size);

            // If allocation fails, leave original block unaffected
            if (!new_block)
                return nullptr;

            // Copy the original to the new block
            memcpy(new_block, block, hdr->size_next);

            // Free the original
            heap_free(heap, block);

            // Return new block
            block = new_block;
        }
    } else if (block && !size) {
        // Reallocating to zero size is equivalent to heap_free
        heap_free(heap, block);
        block = nullptr;
    } else {
        // Reallocating null block is equivalent to heap_alloc
        return heap_alloc(heap, size);
    }

    return block;
}

#endif

#if HEAP_PAGEONLY

struct heap_t {
};

heap_t *heap_create(void)
{
    return (heap_t*)0x42;
}

void heap_destroy(heap_t *)
{
}

__assume_aligned(16)
void *heap_calloc(heap_t *heap, size_t num, size_t size)
{
    void *blk = heap_alloc(heap, num * size);
    return memset(blk, 0, size);
}

// |      ...     |
// +--------------+ <-- mmap return value
// |  Guard Page  |
// +--------------+ <-- 4KB aligned
// |//// 0xFB ////| <-- 0 to 4080 bytes ("fill before")
// +--------------+ <-- 16 byte aligned
// |  heap_hdr_t  |
// +--------------+ <-- 16 byte aligned, return value
// |     data     | <-- 0xF0 filled
// +--------------+
// |//// 0xFA ////| <-- 0 to 15 bytes ("fill after")
// +--------------+ <-- 4KB aligned
// |  Guard Page  |
// +--------------+ <-- mmap size
// |      ...     |

__assume_aligned(16)
void *heap_alloc(heap_t *heap, size_t size)
{
    // Round size up to a multiple of 16 bytes, include header in size
    size = ((size + 15) & -16) + sizeof(heap_hdr_t);

    // Accessible data size in multiples of pages
    size_t reserve = (size + PAGESIZE - 1) & -PAGESIZE;

    // Allocate accessible range plus guard pages at both ends
    size_t alloc = reserve + PAGESIZE * 2;

    // Offset to guard page at end of range
    size_t end_guard = alloc - PAGESIZE;

    // Reserve virtual address space
    char *blk = (char*)mmap(nullptr, alloc,
                            PROT_READ | PROT_WRITE, MAP_NOCOMMIT, -1, 0);

    heap_hdr_t *hdr = (heap_hdr_t*)(blk + (end_guard - size));

    hdr->size_next = size;
    hdr->sig1 = HEAP_BLK_TYPE_USED;
    hdr->sig2 = HEAP_BLK_TYPE_USED ^ uint32_t(size);

    mprotect(blk, PAGESIZE, PROT_NONE);
    mprotect(blk + end_guard, PAGESIZE, PROT_NONE);

    memset(blk + PAGESIZE, 0xFA, (char*)hdr - (blk + PAGESIZE));

    memset(hdr + 1, 0xF0, size - sizeof(heap_hdr_t));

    return hdr + 1;
}

void heap_free(heap_t *heap, void *block)
{
    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;
    assert(hdr->sig1 == HEAP_BLK_TYPE_USED);
    assert(hdr->sig2 == (HEAP_BLK_TYPE_USED ^ uint32_t(hdr->size_next)));
    uintptr_t end = uintptr_t(hdr) + hdr->size_next;
    uintptr_t st = (uintptr_t(hdr) & -PAGESIZE);
    mprotect((void*)st, end - st, PROT_NONE);
}

__assume_aligned(16)
void *heap_realloc(heap_t *heap, void *block, size_t size)
{
    if (unlikely(!block))
        return heap_alloc(heap, size);

    if (unlikely(size == 0)) {
        heap_free(heap, block);
    }

    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;
    char *other = (char*)heap_alloc(heap, size);
    return memcpy(other, block, hdr->size_next - sizeof(heap_hdr_t));
}

#endif
