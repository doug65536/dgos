#include "hash_table.h"
#include "stdlib.h"
#include "hash.h"
#include "string.h"

// Open addressing hash table
// When deleting an item with a non-null next slot,
// we "punch a hole" by setting the item pointer to
// (void*)1. We don't decrease the count when punching
// a hole. We continue probe when encountering a hole
// we really delete items if the next slot is null.
// We don't increase count when inserting at a hole.
// If holes accumulate, this causes a rehash to occur
// appropriately.

void htbl_create(hashtbl_t *self,
                 uint32_t key_ofs,
                 uint32_t key_size)
{
    self->items = 0;
    self->count = 0;
    self->key_ofs = key_ofs;
    self->key_size = key_size;
    self->log2_capacity = 0;
}

void htbl_destroy(hashtbl_t *self)
{
    unique_lock<shared_mutex> lock(self->lock);
    free(self->items);
    self->items = 0;
    self->log2_capacity = 0;
}

static int htbl_rehash(hashtbl_t *self, unique_lock<shared_mutex> const&)
{
    unsigned new_log2 = self->log2_capacity
            ? self->log2_capacity + 1
            : 4;

    size_t new_capacity = 1 << new_log2;
    void **new_tbl = (void**)calloc(new_capacity, sizeof(*self->items));
    if (!new_tbl)
        return 0;

    uint32_t new_mask = ~((uint32_t)-1 << new_log2);
    uint32_t new_count = 0;

    if (self->count) {
        for (uint32_t i = 0, e = 1 << self->log2_capacity;
             i < e; ++i) {
            // Skip nulls and holes
            if (self->items[i] <= (void*)1)
                continue;

            uint32_t hash = hash_32((char*)self->items[i] +
                                    self->key_ofs, self->key_size);

            hash &= new_mask;

            // Probe and insert
            for (uint32_t k = 0; k < new_capacity;
                 ++k, hash = (hash + 1) & new_mask) {
                if (!new_tbl[k]) {
                    new_tbl[hash] = self->items[i];
                    ++new_count;
                    break;
                }
            }
        }
    }

    free(self->items);
    self->log2_capacity = new_log2;
    self->items = new_tbl;
    self->count = new_count;

    return 1;
}

void *htbl_lookup(hashtbl_t *self, void *key)
{
    void *item = nullptr;

    shared_lock<shared_mutex> lock(self->lock);
    if (self->count) {
        uint32_t hash = hash_32(key, self->key_size);
        uint32_t mask = ~((uint32_t)-1 << self->log2_capacity);

        hash &= mask;

        for (uint32_t k = 0, e = 1 << self->log2_capacity;
             k < e; ++k, hash = (hash + 1) & mask) {
            if (self->items[hash] > (void*)1) {
                void *check = (char*)self->items[hash] +
                        self->key_ofs;
                if (!memcmp(check, key, self->key_size)) {
                    item = self->items[hash];
                    break;
                }
            } else if (!self->items[hash]) {
                break;
            }
        }
    }

    return item;
}

int htbl_insert(hashtbl_t *self, void *item)
{
    unique_lock<shared_mutex> lock(self->lock);

    if (!self->items ||
            self->count >= ((1U<<self->log2_capacity) * 3U) >> 2) {
        if (!htbl_rehash(self, lock))
            return 0;
    }

    uint32_t hash = hash_32((char*)item + self->key_ofs,
                            self->key_size);
    uint32_t mask = ~((uint32_t)-1 << self->log2_capacity);

    hash &= mask;

    for (uint32_t k = 0, e = 1 << self->log2_capacity;
         k < e; ++k, hash = (hash + 1) & mask) {
        if (self->items[hash] <= (void*)1) {
            // Increase count if it wasn't deletion hole
            self->count += (self->items[hash] == 0);
            self->items[hash] = item;
            break;
        }
    }

    return 1;
}

void htbl_delete(hashtbl_t *self, void *key)
{
    shared_lock<shared_mutex> lock(self->lock);
    if (self->count) {
        uint32_t hash = hash_32(key, self->key_size);
        uint32_t mask = ~(uint32_t(-1) << self->log2_capacity);

        hash &= mask;

        for (uint32_t k = 0, e = 1U << self->log2_capacity;
             k < e; ++k, hash = (hash + 1) & mask) {
            if (self->items[hash] > (void*)1) {
                void const *check = (char const *)self->items[hash] +
                        self->key_ofs;

                if (!memcmp(check, key, self->key_size)) {
                    if (self->items[(hash + 1) & mask]) {
                        // Next item is not null, punch hole
                        self->items[hash] = (void*)1;
                    } else {
                        // Next item is null, really delete
                        self->items[hash] = nullptr;
                        --self->count;
                    }
                    break;
                }
            } else if (!self->items[hash]) {
                break;
            }
        }
    }
}
