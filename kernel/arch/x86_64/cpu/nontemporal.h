#pragma once
#include "types.h"

KERNEL_API void *memcpy512_nt(void *dest, void const *src, size_t n);
KERNEL_API void *memcpy32_nt(void *dest, void const *src, size_t n);
KERNEL_API void *memset32_nt(void *dest, uint32_t val, size_t n);
KERNEL_API void memcpy_nt_fence(void);
