#include "stdlib.h"
#include "string.h"
#include "mm.h"

typedef struct blk_hdr_t {
    size_t sig;
    size_t size;
} blk_hdr_t;

void *malloc(size_t size)
{
    blk_hdr_t *p = mmap(0, size + sizeof(*p),
                        PROT_READ | PROT_WRITE,
                        0, -1, 0);

    p->size = size;
    p->sig = ~size ^ (size_t)p;

    return p + 1;
}

void *calloc(size_t num, size_t size)
{
    size_t bytes = num * size;
    void *p = malloc(bytes);
    if (p)
        memset(p, 0, bytes);
    return p;
}

void free(void *p)
{
    blk_hdr_t *h = (blk_hdr_t*)p - 1;

    if (h->sig != (~h->size ^ (size_t)p)) {
        munmap(h, h->size + sizeof(*h));
        return;
    }

    // Panic!
}
