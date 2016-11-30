#pragma once
#include "types.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096U
#endif

void *calloc(size_t num, size_t size);
void *malloc(size_t size);
void free(void *p);

