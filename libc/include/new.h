#pragma once
#include <stdlib.h>

#ifdef __cplusplus

// XXX: why do I need extern "C++"???
extern "C++" void *operator new(size_t size);
extern "C++" void *operator new(size_t size, void *p) noexcept;
extern "C++" void operator delete(void *block);
extern "C++" void operator delete(void *block, unsigned long size);
extern "C++" void operator delete[](void *block) noexcept;
extern "C++" void operator delete[](void *block, unsigned int) noexcept;

#endif
