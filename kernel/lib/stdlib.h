#pragma once
#include "types.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096U
#endif

__attribute__((malloc))
void *calloc(size_t num, size_t size);

__attribute__((malloc))
void *malloc(size_t size);

void free(void *p);

