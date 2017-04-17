#pragma once
#include "types.h"
#include "cpu/spinlock.h"

struct hashtbl_t {
    void **items;
    uint32_t count;
    uint32_t key_ofs;
    uint32_t key_size;
    uint8_t log2_capacity;
    rwspinlock_t lock;
};

void htbl_create(hashtbl_t *self,
                  uint32_t key_ofs, uint32_t key_size);
void htbl_destroy(hashtbl_t *self);
void *htbl_lookup(hashtbl_t *self, void *key);
void htbl_delete(hashtbl_t *self, void *key);
int htbl_insert(hashtbl_t *self, void *item);
