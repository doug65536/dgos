#include "pool.h"
#include "mm.h"
#include "bitsearch.h"
#include "string.h"

static uint32_t pool_round_up(uint32_t n, int8_t log2m)
{
    return (n + ((1<<log2m)-1)) & ((uint32_t)-1 << log2m);
}

int pool_create(pool_t *pool, uint32_t item_size, uint32_t capacity)
{
    // Cache line align first item slot
    size_t hdr_size = pool_round_up(sizeof(pool_t), POOL_LOG2_ALIGN);

    // Round item size up to multiple of cache line
    item_size = pool_round_up(item_size, POOL_LOG2_ALIGN);

    size_t ext_cap = item_size * capacity;
    size_t size = hdr_size + ext_cap;

    void *items = mmap(0, size, PROT_READ | PROT_WRITE,
                       MAP_UNINITIALIZED, -1, 0);

    if (!pool || pool == MAP_FAILED)
        return 0;

    pool->items = (char*)items + hdr_size;
    pool->item_count = 1;
    pool->item_capacity = capacity;
    pool->item_size = item_size;
    pool->first_free = ~0;
    mutex_init(&pool->lock);
    pool->first_free = 0;

    return 1;
}

void pool_destroy(pool_t *pool)
{
    // Cache line align first item slot
    size_t hdr_size = pool_round_up(sizeof(pool_t), POOL_LOG2_ALIGN);

    size_t ext_cap = pool->item_size * pool->item_capacity;
    size_t size = hdr_size + ext_cap;

    mutex_destroy(&pool->lock);

    munmap(pool, size);
}

static void *pool_item(pool_t *pool, uint32_t index)
{
    assert(index != 0);
    return pool->items + index * pool->item_size;
}

void *pool_alloc(pool_t *pool)
{
    uint32_t *slot;

    mutex_lock(&pool->lock);

    if (pool->first_free && pool->first_free != ~0U) {
        // Reuse freed item
        slot = (uint32_t*)pool_item(pool, pool->first_free);
        pool->first_free = *slot;
    } else if (pool->item_count < pool->item_capacity) {
        // Use a new slot
        slot = (uint32_t*)pool_item(pool, pool->item_count++);
    } else {
        // Full
        slot = 0;
    }

    mutex_unlock(&pool->lock);

    return slot;
}

void *pool_calloc(pool_t *pool)
{
    void *item = pool_alloc(pool);
    memset(item, 0, pool->item_size);
    return item;
}

void pool_free(pool_t *pool, void *item)
{
    mutex_lock(&pool->lock);

    size_t ofs = (char*)item - pool->items;
    uint32_t index = ofs / pool->item_size;
    *(uint32_t*)item = pool->first_free;
    pool->first_free = index;

    mutex_unlock(&pool->lock);
}

