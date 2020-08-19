#pragma once

#include "types.h"
#include "physmem.h"
#include "paging.h"

physmem_range_t *physmap_get(size_t *ret_count);
int physmap_insert(physmem_range_t const& entry);
int physmap_take_range(uint64_t base, uint64_t size, uint32_t type);
uint64_t physmap_top_addr();
char const *physmap_validate(bool fix = false);
