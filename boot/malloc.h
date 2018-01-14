#pragma once

#include "types.h"

void *malloc(uint16_t bytes);
void *calloc(unsigned num, unsigned size);
void free(void *p);

uint16_t far_malloc(uint32_t bytes);
uint16_t far_malloc_aligned(uint32_t bytes);

#ifndef NDEBUG
void test_malloc();
#endif

void *operator new(size_t size) noexcept;
void *operator new(size_t size, void *p) noexcept;
void operator delete(void *block, unsigned long size) noexcept;
void operator delete(void *block) noexcept;
void operator delete[](void *block) noexcept;
void operator delete[](void *block, unsigned int) noexcept;
