#pragma once

#include "types.h"

void *malloc(uint16_t bytes);
void *calloc(uint16_t num, uint16_t size);
void free(void *p);

uint16_t far_malloc(uint32_t bytes);
uint16_t far_malloc_aligned(uint32_t bytes);

#ifndef NDEBUG
void test_malloc(void);
#endif
