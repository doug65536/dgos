#pragma once
#include "types.h"

#ifndef PAGE_SCALE
#define PAGE_SCALE  12
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096UL
#endif

#define _MALLOC_OVERHEAD    16

extern "C" {

__attribute__((malloc))
void *calloc(size_t num, size_t size);

__attribute__((malloc))
void *malloc(size_t size);

void *realloc(void *p, size_t new_size);

void free(void *p);

char *strdup(char const *s);

void auto_free(void *mem);

#define autofree __attribute__((cleanup(auto_free)))
}

void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *block, unsigned long size);
void operator delete(void *block) throw();
