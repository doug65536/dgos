#include <stdlib.h>
#include <stdint.h>
//#include "../../kernel/lib/cc/algorithm.h"
#include <sys/mman.h>
#include <sys/likely.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <new>

static constexpr const size_t arena_magic = size_t(0x0A0E0A000A0E0A00U);
static constexpr const size_t used_magic = size_t(0xa10cb10c0cb10ca1U);
static constexpr const size_t free_magic = size_t(0xfeeeb10c0cb1eefeU);

struct arena_t;

// 32 bytes on 64-bit, 16 bytes on 32-bit
struct blk_t {
    size_t magic = used_magic;
    blk_t *next_free = nullptr;
    arena_t *arena = nullptr;
    size_t size = 0;
};

static_assert(((sizeof(blk_t)) & (sizeof(blk_t) - 1)) == 0,
              "sizeof(blk_t) must be a power of two");

// Cache line sized
struct arena_t {
    arena_t *next_arena = nullptr;

    arena_t *heap = nullptr;

    uint64_t magic = arena_magic;

    blk_t *first_free = nullptr;

    // with 64 byte minimum block size,
    // 16 bit count limits each arena to 2^(16+6)=2^22=4MB

    // The number of free blocks in an arena is tracked
    // for garbage collecting completely unused arenas
    uint16_t free_count = 0;
    uint16_t capacity = 0;

    // New arenas don't link all the blocks into the free list
    // instead they bump allocate until they are all taken
    // You bump allocate from the latest arena only
    // when the corresponding free block chain is empty
    // If the free block chain is empty and the last arena is
    // fully bumped, then make a new arena for that size and
    // make it the latest arena
    uint16_t bump_alloc = 0;
    uint16_t reserved = 0;

    uint64_t reserved2 = 0;

    pthread_mutex_t arena_lock = PTHREAD_MUTEX_INITIALIZER;
};

#define HEAP_ARENA_SZ   65536
#define HEAP_BIN_COUNT  10
#define HEAP_LOG2_SMALL 6
#define HEAP_LOG2_LARGE (HEAP_LOG2_SMALL + HEAP_BIN_COUNT)

// bin  log2  payload
// [0]     6       32
// [1]     7       96
// [2]     8      224
// [3]     9      480
// [4]    10      992
// [5]    11     2016
// [6]    12     4064
// [7]    13     8160
// [8]    14    16352
// [9]    15    32736
// ->mmap
// (32 bit build has 16 bytes extra payload for each bin)

class heap_t {
public:
    void *alloc(size_t sz);

    static void free(void *block);

private:
    arena_t *create_arena(unsigned bin);

    // First arena for each size
    arena_t *arena_chain[HEAP_BIN_COUNT] = {};

    // Points to current arena with free space, walks through arena
    // list looking for one with space when you attempt to allocate
    // from a fully allocated arena
    arena_t *rovers[HEAP_BIN_COUNT] = {};

    pthread_mutex_t heap_lock = PTHREAD_MUTEX_INITIALIZER;
    arena_t *find_arena_with_space(unsigned bin) const;
};

__thread heap_t thread_heap;

static inline int bit_msb_set(size_t n)
{
    return (sizeof(n) * __CHAR_BIT__ - 1) - __builtin_clzl(n);
}

static inline int bit_log2_n(size_t n)
{
    uint8_t top = bit_msb_set(n);
    return top + !!(~(~UINT32_C(0) << top) & n);
}

static unsigned bin_from_sz(size_t sz)
{
    int log2_sz = bit_log2_n(sz);

    // If too small, use bin 0,
    // else if too large, return bin count (index out of range of array)
    // else return index of bin
    log2_sz = (log2_sz < HEAP_LOG2_SMALL)
            ? (0)
            : (log2_sz >= HEAP_LOG2_LARGE)
              ? (HEAP_LOG2_LARGE - HEAP_LOG2_SMALL)
              : (log2_sz - HEAP_LOG2_SMALL);

    return log2_sz;
}

static void *err_no_mem()
{
    errno = ENOMEM;
    return nullptr;
}

void *malloc(size_t sz)
{
    return thread_heap.alloc(sz);
}

void free(void *p)
{
    heap_t::free(p);
}

// Caller is expected to have been holding the heap lock, returns
// with the heap locked
arena_t *heap_t::create_arena(unsigned bin)
{
    if (bin >= HEAP_LOG2_LARGE)
        __builtin_unreachable();

    // Release the lock while allocating memory for new arena
    pthread_mutex_unlock(&heap_lock);

    void *mem = mmap(nullptr, HEAP_ARENA_SZ,
                     PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);

    if (unlikely(!mem))
        return (arena_t*)err_no_mem();

    arena_t *arena = new (mem) arena_t();

    char *payload_st = (char*)(arena + 1);

    char *payload_en = payload_st + (HEAP_ARENA_SZ - sizeof(arena_t));

    size_t payload_sz = payload_en - payload_st;

    // Divide the payload area size by the item size
    arena->capacity = payload_sz >> (bin + HEAP_LOG2_SMALL);
    arena->free_count = arena->capacity;

    pthread_mutex_lock(&heap_lock);

    // Link arena into arena chain
    arena->next_arena = arena_chain[bin];
    arena_chain[bin] = arena;

    // New arena is the current arena for allocation
    rovers[bin] = arena;

    return arena;
}

arena_t *heap_t::find_arena_with_space(unsigned bin) const
{
    arena_t *arena = arena_chain[bin];

    while (arena && arena->free_count == 0)
        arena = arena->next_arena;

    return arena;
}

void *heap_t::alloc(size_t sz)
{
    // Make room for header
    // Round up to multiple of header size
    sz = (sizeof(blk_t) + sz + (sizeof(blk_t) - 1)) & -sizeof(blk_t);

    unsigned bin = bin_from_sz(sz);

    blk_t *block;

    if (bin < HEAP_BIN_COUNT) {
        pthread_mutex_lock(&heap_lock);

        arena_t *arena = rovers[bin];

        if (likely(arena)) {
            if (unlikely(arena->free_count == 0))
                arena = find_arena_with_space(bin);
        } else {
            arena = create_arena(bin);
        }

        if (unlikely(!arena))
            return err_no_mem();

        // Definitely have arena here

        if (arena->bump_alloc < arena->capacity) {
            size_t offset;

            offset = size_t(arena->bump_alloc++) << (bin + HEAP_LOG2_SMALL);

            void *mem = (char*)(arena + 1) + offset;

            block = new (mem) blk_t();
        } else {
            block = arena->first_free;

            arena->first_free = block->next_free;
            assert(block->magic == free_magic);
            block->magic = used_magic;
        }

        --arena->free_count;
        block->arena = arena;
        block->next_free = nullptr;

        pthread_mutex_unlock(&heap_lock);
    } else {
        // Large allocations go directly to a syscall, no usermode locks
        void *mem = mmap(nullptr, sz, PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_ANONYMOUS, -1, 0);

        if (unlikely(mem == MAP_FAILED))
            return err_no_mem();

        block = new (mem) blk_t();

        // Not part of any arena

    }

    block->size = sz;

    return block + 1;
}

void heap_t::free(void *block)
{
    blk_t *blk = (blk_t*)block - 1;

    assert(blk->magic == used_magic);

    arena_t *blk_arena = blk->arena;

    pthread_mutex_lock(&blk_arena->arena_lock);

    if (++blk_arena->free_count == blk_arena->capacity) {
        // Whole thing is free, switch back to bump allocator and reset it
        blk_arena->bump_alloc = 0;

        // Empty arena free list
        blk_arena->first_free = nullptr;
    } else {
        // Link into arena-local free chain
        blk->next_free = blk_arena->first_free;
        blk_arena->first_free = blk;
    }

    pthread_mutex_unlock(&blk_arena->arena_lock);
}
