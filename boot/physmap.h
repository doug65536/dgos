#pragma once

#include "types.h"
#include "physmem.h"
#include "paging.h"

bool physmap_init();
void physmap_clear();
physmem_range_t *physmap_get(int *ret_count);
void physmap_realloc(int capacity_hint);
void physmap_delete(int index);
int physmap_insert(physmem_range_t const& entry);
int physmap_replace(int index, physmem_range_t const& entry);
