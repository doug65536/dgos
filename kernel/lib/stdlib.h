#pragma once
#include "types.h"

#ifndef PAGE_SCALE
#define PAGE_SCALE  12
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096UL
#endif

#define _MALLOC_OVERHEAD    16

#ifdef __cplusplus
struct bad_alloc
{
};
#endif

__BEGIN_DECLS

void malloc_startup(void *p);

_malloc _assume_aligned(16) _alloc_size(1, 2)
void *calloc(size_t num, size_t size);

_malloc _assume_aligned(16) _alloc_size(1)
void *malloc(size_t size);

_assume_aligned(16) _alloc_size(2)
void *realloc(void *p, size_t new_size);

void free(void *p);

_malloc _assume_aligned(16)
char *strdup(char const *s);

int strtoi(char const *str, char **end, int base);
long strtol(char const *str, char **end, int base);
long long strtoll(char const *str, char **end, int base);

int atoi(char const *str);
int atol(char const *str);
int atoll(char const *str);

__END_DECLS

_const
void *operator new(size_t, void *p) noexcept;

_malloc _assume_aligned(16)
void *operator new(size_t size);

_malloc _assume_aligned(16)
void *operator new[](size_t size);

void operator delete(void *block, unsigned long size) noexcept;

class zero_init_t
{
public:
    void *operator new(size_t size)
    {
        return calloc(1, size);
    }

    void operator delete(void *p)
    {
        free(p);
    }
};
//void operator delete(void *block) throw();
//void operator delete[](void *block) throw();
