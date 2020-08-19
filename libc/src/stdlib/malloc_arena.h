#pragma once
#include <pthread.h>
#include <stdint.h>

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
