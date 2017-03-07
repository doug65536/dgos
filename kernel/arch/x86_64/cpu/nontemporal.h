#pragma once
#include "types.h"

void *memcpy512_nt(void *dest, void const *src, size_t n);
void *memcpy32_nt(void *dest, void const *src, size_t n);
void memset32_nt(void *dest, uint32_t val, size_t n);
void memcpy_nt_fence(void);
