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

void operator delete(void *block, unsigned long size)
{
    (void)size;
    free(block);
}

void operator delete(void *block) throw()
{
    free(block);
}
