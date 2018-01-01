#pragma once

#include "types.h"
#include "cpu/isr.h"

struct physmem_range_t {
    uint64_t base;
    uint64_t size;
    uint32_t type;
    uint32_t valid;
};

#define PHYSMEM_TYPE_NORMAL         1
#define PHYSMEM_TYPE_UNUSABLE       2
#define PHYSMEM_TYPE_RECLAIMABLE    3
#define PHYSMEM_TYPE_NVS            4
#define PHYSMEM_TYPE_BAD            5

typedef uintptr_t physaddr_t;
typedef uintptr_t linaddr_t;

extern physmem_range_t *phys_mem_map;
extern size_t phys_mem_map_count;
extern physaddr_t root_physaddr;

extern "C" void mmu_init();
uintptr_t mm_create_process(void);
void mm_destroy_process(void);

extern "C" isr_context_t *mmu_page_fault_handler(int intr, isr_context_t *ctx);
