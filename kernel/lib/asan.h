#pragma once
#include "types.h"

#ifdef ASAN_ENABLED
extern "C" void __asan_freeN_noabort(void *m, size_t sz)
#else
#define __asan_freeN_noabort(p, n) ((void)0)
#endif
