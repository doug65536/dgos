#include "pool.h"
#include "mm.h"
#include "bitsearch.h"
#include "string.h"
#include "stdlib.h"

// Isolate pool items onto separate cache lines
#define POOL_LOG2_ALIGN     6
#define POOL_ALIGN          (1U<<POOL_LOG2_ALIGN)

C_ASSERT(sizeof(pool_base_t) < POOL_ALIGN);

static uint32_t pool_round_up(uint32_t n, int8_t log2m)
{
    return (n + ((1U<<log2m)-1)) & (~0U << log2m);
}

bool pool_base_t::create(uint32_t item_size, uint32_t capacity)
{
    // Round item size up to multiple of cache line
    item_size = pool_round_up(item_size, POOL_LOG2_ALIGN);

    size_t size = item_size * capacity;

    void *pool_mem = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                          MAP_POPULATE);

    if (unlikely(!pool_mem || pool_mem == MAP_FAILED))
        return false;

    items = (char*)pool_mem;
    item_count = 0;
    item_capacity = capacity;
    item_size = item_size;
    first_free = -1U;

    return true;
}

pool_base_t::pool_base_t()
    : items(nullptr)
    , item_size(0)
    , item_capacity(0)
    , item_count(0)
    , first_free(0)
{
}

pool_base_t::~pool_base_t()
{
    // Cache line align first item slot
    size_t hdr_size = pool_round_up(sizeof(pool_base_t), POOL_LOG2_ALIGN);

    size_t ext_cap = item_size * item_capacity;
    size_t size = hdr_size + ext_cap;

    munmap(this, size);
}

void *pool_base_t::item(uint32_t index)
{
    assert(index < item_capacity);
    return items + index * item_size;
}

void *pool_base_t::alloc()
{
    void *slot;

    scoped_lock lock(pool_lock);

    if (first_free != ~0U) {
        // Reuse freed item
        slot = (uint32_t*)item(first_free);
        // Items in the free chain have next free index
        // patched into 1st 4 bytes
        memcpy(&first_free, slot, sizeof(first_free));
    } else if (item_count < item_capacity) {
        // Use a new slot
        slot = (uint32_t*)item(item_count++);
    } else {
        // Full
        slot = nullptr;
    }

#if HEAP_DEBUG
    if (likely(slot))
        memset(slot, 0xf0, item_size);
#endif

    return slot;
}

void *pool_base_t::calloc()
{
    void *slot = alloc();

    if (likely(slot))
        memset(slot, 0, item_size);

    return slot;
}

void pool_base_t::free(uint32_t index)
{
    scoped_lock lock(pool_lock);

    void *slot = item(index);

#if HEAP_DEBUG
    memset(slot, 0xfe, item_size);
#endif

    // Patch next link index into block memory
    memcpy(slot, &first_free, sizeof(first_free));

    first_free = index;
}

