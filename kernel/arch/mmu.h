#pragma once

#include "types.h"

typedef struct physmem_range_t {
    uint64_t base;
    uint64_t size;
    uint32_t type;
    uint32_t valid;
} physmem_range_t;

#define PHYSMEM_TYPE_NORMAL         1
#define PHYSMEM_TYPE_UNUSABLE       2
#define PHYSMEM_TYPE_RECLAIMABLE    3
#define PHYSMEM_TYPE_NVS            4
#define PHYSMEM_TYPE_BAD            5

extern physmem_range_t *phys_mem_map;
extern size_t phys_mem_map_count;

void mmu_init(void);
