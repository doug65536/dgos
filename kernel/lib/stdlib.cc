#include "stdlib.h"
#include "string.h"
#include "mm.h"
#include "assert.h"
#include "printk.h"
#include "heap.h"
#include "callout.h"

// Always use paged allocation with guard pages
// Realloc always moves the memory to a new range
#define HEAP_PAGEONLY 1

#if !HEAP_PAGEONLY
static heap_t *default_heap;

void malloc_startup(void *p)
{
    (void)p;
    default_heap = heap_create();
}

void *calloc(size_t num, size_t size)
{
    return heap_calloc(default_heap, num, size);
}

void *malloc(size_t size)
{
    return heap_alloc(default_heap, size);
}

void *realloc(void *p, size_t new_size)
{
    return heap_realloc(default_heap, p, new_size);
}

void free(void *p)
{
    heap_free(default_heap, p);
}
#else

void malloc_startup(void *p)
{
}

void *calloc(size_t num, size_t size)
{
    return pageheap_calloc(num, size);
}

void *malloc(size_t size)
{
    return pageheap_alloc(size);
}

void *realloc(void *p, size_t new_size)
{
    return pageheap_realloc(p, new_size);
}

void free(void *p)
{
    pageheap_free(p);
}

#endif

char *strdup(char const *s)
{
    size_t len = strlen(s);
    char *b = new char[len+1];
    return (char*)memcpy(b, s, len+1);
}

void auto_free(void *mem)
{
    void *blk = *(void**)mem;
    if (blk) {
        free(blk);
        *(void**)mem = nullptr;
    }
}

void *operator new(size_t size) noexcept
{
    return malloc(size);
}

void *operator new[](size_t size) noexcept
{
    return malloc(size);
}

void operator delete(void *block, size_t) noexcept
{
    free(block);
}

void operator delete(void *block) throw()
{
    free(block);
}

void operator delete[](void *block) noexcept
{
    free(block);
}

void operator delete[](void *block, size_t) noexcept
{
    free(block);
}

_const
void *operator new(size_t, void *p) noexcept
{
    return p;
}

// FIXME: Overflow is not handled properly
template<typename T>
static T strto(char const *str, char **end, int base)
{
    T n = 0;
    T digit;
    T sign = 0;

    for (;; ++str) {
        char c = *str;

        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'Z')
            digit = c + 10 - 'A';
        else if (c >= 'a' && c <= 'z')
            digit = c + 10 - 'a';
        else if (c == '-' && sign == 0) {
            sign = -1;
            continue;
        } else if (c == '+' && sign == 0) {
            sign = 1;
            continue;
        }
        else
            break;

        if (digit >= base)
            break;

        ++str;
        n *= base;
        n += digit;
    }

    if (end)
        *end = (char*)str;

    return !sign ? n : n * sign;
}

int strtoi(char const *str, char **end, int base)
{
    return strto<int>(str, end, base);
}

long strtol(char const *str, char **end, int base)
{
    return strto<long>(str, end, base);
}

long long strtoll(char const *str, char **end, int base)
{
    return strto<long long>(str, end, base);
}
