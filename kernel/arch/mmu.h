#pragma once

#include "types.h"

typedef struct {
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

void mmu_init(void);

void mmu_set_fsgsbase(void *fs_base, void *gs_base);
