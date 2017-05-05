#include "stdlib.h"
#include "string.h"
#include "mm.h"
#include "assert.h"
#include "printk.h"
#include "heap.h"
#include "callout.h"

static heap_t *default_heap;

static void malloc_startup(void *p)
{
    (void)p;
    default_heap = heap_create();
}

REGISTER_CALLOUT(malloc_startup, 0, 'M', "000");

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
        *(void**)mem = 0;
    }
}

void *operator new(size_t size)
{
    return malloc(size);
}

void *operator new[](size_t size)
{
    return malloc(size);
}

void operator delete(void *block, size_t)
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

void *operator new(size_t, void *p)
{
    return p;
}

// FIXME: Overflow is not handled properly
template<typename T>
static T strto(const char *str, char **end, int base)
{
    T n = 0;
    T digit;
    T sign = 0;

    for (;;) {
        char c = *str;

        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'Z')
            digit = c + 10 - 'A';
        else if (c >= 'a' && c <= 'z')
            digit = c + 10 - 'a';
        else if (c == '-' && sign == 0)
            sign = -1;
        else if (c == '+' && sign == 0)
            sign = 1;
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

int strtoi(const char *str, char **end, int base)
{
    return strto<int>(str, end, base);
}

long strtol(const char *str, char **end, int base)
{
    return strto<long>(str, end, base);
}

long long strtoll(const char *str, char **end, int base)
{
    return strto<long long>(str, end, base);
}
