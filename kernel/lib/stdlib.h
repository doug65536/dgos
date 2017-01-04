#pragma once
#include "types.h"

#ifndef PAGE_SCALE
#define PAGE_SCALE  12
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096U
#endif

#define _MALLOC_OVERHEAD    16

__attribute__((malloc))
void *calloc(size_t num, size_t size);

__attribute__((malloc))
void *malloc(size_t size);

void *realloc(void *p, size_t new_size);

void free(void *p);

