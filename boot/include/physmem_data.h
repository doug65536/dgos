#pragma once

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

// Custom types
#define PHYSMEM_TYPE_ALLOCATED      6
#define PHYSMEM_TYPE_BOOTLOADER     7
