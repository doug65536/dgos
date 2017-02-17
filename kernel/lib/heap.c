#include "heap.h"
#include "mm.h"
#include "assert.h"
#include "string.h"
#include "threadsync.h"
#include "bitsearch.h"
#include "printk.h"

typedef struct heap_hdr_t {
    uintptr_t size_next;
    uint32_t sig1;
    uint32_t sig2;
} heap_hdr_t;

#define HEAP_BLK_TYPE_USED  0xa10ca1ed
#define HEAP_BLK_TYPE_FREE  0xfeeeb10c

/// [ 0] ->      32
/// [ 1] ->      64
/// [ 2] ->     128
/// [ 3] ->     256
/// [ 4] ->     512
/// [ 5] ->    1024
/// [ 6] ->    2048
/// [ 7] ->    4096
/// [ 8] ->    8096
/// [ 9] ->   16384
/// [10] ->   32768
/// [11] ->   65536
/// .... -> use mmap

#define HEAP_BUCKET_COUNT   12
#define HEAP_MMAP_THRESHOLD (1<<(5+HEAP_BUCKET_COUNT-1))

#define HEAP_BUCKET_SIZE    (1<<(HEAP_BUCKET_COUNT+5-1))
#define HEAP_MAX_ARENAS \
    (((PAGESIZE - \
    sizeof(void*) * HEAP_BUCKET_COUNT - \
    sizeof(heap_ext_arena_t*) - \
    sizeof(mutex_t)) / \
    sizeof(void*)) - 1)

// When the main heap_t::arenas array overflows, an additional
// page is allocated to hold additional arena pointers.
typedef struct heap_ext_arena_t heap_ext_arena_t;
struct heap_ext_arena_t {
    heap_ext_arena_t *prev;
    size_t arena_count;
    void *arenas[(PAGESIZE - sizeof(heap_ext_arena_t*) -
                  sizeof(size_t)) / sizeof(void*)];
};

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
    heap_t *heap = mmap(0, sizeof(heap_t),
                        PROT_READ | PROT_WRITE, 0, -1, 0);
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

        new_list = mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                        0, -1, 0);
        if (!new_list || new_list == MAP_FAILED)
            return 0;

        new_list->prev = heap->last_ext_arena;
        new_list->arena_count = 0;
        heap->last_ext_arena = new_list;

        arena_count_ptr = &new_list->arena_count;
        arena_list_ptr = new_list->arenas;
    }

    size_t bucket = log2size - 5;

    char *arena = mmap(0, HEAP_BUCKET_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_POPULATE, -1, 0);
    char *arena_end = arena + HEAP_BUCKET_SIZE;

    arena_list_ptr[(*arena_count_ptr)++] = arena;

    size_t size = 1 << log2size;

    heap_hdr_t *hdr = 0;
    for (char *fill = arena_end - size; fill >= arena; fill -= size) {
        hdr = (void*)fill;
        hdr->size_next = (uintptr_t)heap->free_chains[bucket];
        heap->free_chains[bucket] = hdr;
    }

    return hdr;
}

void *heap_calloc(heap_t *heap, size_t num, size_t size)
{
    size *= num;
    void *block = heap_alloc(heap, size);

    if (size < HEAP_MMAP_THRESHOLD)
        return memset(block, 0, size);

    // mmap already guarantees cleared pages
    return block;
}

static void *heap_large_alloc(size_t size)
{
    heap_hdr_t *hdr = mmap(0, size,
                           PROT_READ | PROT_WRITE,
                           0, -1, 0);
    hdr->size_next = size;
    hdr->sig1 = HEAP_BLK_TYPE_USED;
    hdr->sig2 = HEAP_BLK_TYPE_USED ^ (uint32_t)size;

    return hdr + 1;
}

static void heap_large_free(heap_hdr_t *hdr, size_t size)
{
    munmap(hdr, size);
}

void *heap_alloc(heap_t *heap, size_t size)
{
    if (size == 0)
        return 0;

    // Add room for bucket header
    size += sizeof(heap_hdr_t);

    size_t orig_size = size;

    // Calculate ceil(log(size) / log(2))
    uint8_t log2size = bit_log2_n_64(size);

    // Round up to bucket item size
    size = 1 << log2size;

    // Bucket 0 is 32 bytes
    size_t bucket = log2size - 5;

    heap_hdr_t *first_free;

    if (bucket < HEAP_BUCKET_COUNT) {
        mutex_lock(&heap->lock);

        // Try to take a free item
        first_free = heap->free_chains[bucket];
    } else {
        return heap_large_alloc(orig_size);
    }

    if (!first_free) {
        // Create a new bucket
        first_free = heap_create_arena(heap, log2size);
    }

    // Remove block from chain
    heap->free_chains[bucket] = (void*)first_free->size_next;

    mutex_unlock(&heap->lock);

    if (first_free) {
        // Store size in header
        first_free->size_next = size;

        first_free->sig1 = HEAP_BLK_TYPE_USED;
        first_free->sig2 = HEAP_BLK_TYPE_USED ^ (uint32_t)size;

        return first_free + 1;
    }

    return 0;
}

void heap_free(heap_t *heap, void *block)
{
    if (!block)
        return;

    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

    assert(hdr->sig1 == HEAP_BLK_TYPE_USED);

    uint8_t log2size = bit_log2_n_32((int32_t)hdr->size_next);
    assert(log2size >= 5 && log2size < 32);
    size_t bucket = log2size - 5;

    if (bucket < HEAP_BUCKET_COUNT) {
        assert(hdr->sig2 == (HEAP_BLK_TYPE_USED ^ (1 << log2size)));

        hdr->sig1 = HEAP_BLK_TYPE_FREE;

        mutex_lock(&heap->lock);
        hdr->size_next = (uintptr_t)heap->free_chains[bucket];
        heap->free_chains[bucket] = hdr;
        mutex_unlock(&heap->lock);
    } else {
        heap_large_free(hdr, hdr->size_next);
    }    
}

void *heap_realloc(heap_t *heap, void *block, size_t size)
{
    if (block) {
        heap_hdr_t *hdr = (heap_hdr_t*)block - 1;
        uint8_t newlog2size = bit_log2_n_32((int32_t)size);
        size_t new_size = 1 << newlog2size;
        if (hdr->size_next >= new_size) {
            void *new_block = heap_alloc(heap, size);
            memcpy(new_block, block, hdr->size_next);
            heap_free(heap, block);
            block = new_block;
        }
        return block;
    }
    return heap_alloc(heap, size);
}

