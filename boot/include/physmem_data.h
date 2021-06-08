#pragma once
#include "types.h"
#include "assert.h"

#define PHYSMEM_TYPE_NORMAL         1
#define PHYSMEM_TYPE_UNUSABLE       2
#define PHYSMEM_TYPE_RECLAIMABLE    3
#define PHYSMEM_TYPE_NVS            4
#define PHYSMEM_TYPE_BAD            5

// Custom types
#define PHYSMEM_TYPE_ALLOCATED      6
#define PHYSMEM_TYPE_BOOTLOADER     7
#define PHYSMEM_TYPE_NORMAL_2M      8
#define PHYSMEM_TYPE_NORMAL_1G      9

struct physmem_range_t {
    uint64_t base;
    uint64_t size;
    uint32_t type;
    uint32_t valid;

    uint64_t end() const
    {
        return base + size;
    }

    void set_end(uint64_t end)
    {
        assert(end >= base);

        size = end - base;
    }

    void set_start(uint64_t start)
    {
        assert(start < end());

        int64_t adj = int64_t(base - start);

        size += adj;
        base = start;
    }

    constexpr bool is_normal() const noexcept
    {
        return (type == PHYSMEM_TYPE_NORMAL) ||
                (type == PHYSMEM_TYPE_NORMAL_1G) ||
                (type == PHYSMEM_TYPE_NORMAL_2M);
    }
};

static char const *physmem_names[] = {
    "<zero>",
    "NORMAL",
    "UNUSABLE",
    "RECLAIMABLE",
    "NVS",
    "BAD",

    "ALLOCATED",
    "BOOTLOADER",

    "NORMAL_2M",
    "NORMAL_1G"
};

static size_t const physmem_names_count = countof(physmem_names);
