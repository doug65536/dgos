#include "stdlib.h"
#include "string.h"
#include "mm.h"
#include "assert.h"

typedef struct blk_hdr_t {
    size_t sig;
    size_t size;
} blk_hdr_t;

C_ASSERT(sizeof(blk_hdr_t) == _MALLOC_OVERHEAD);

static void chk_sig(blk_hdr_t *p)
{
    assert(p->sig == (~p->size ^ (uintptr_t)p));
}

static void set_sig(blk_hdr_t *p)
{
    p->sig = ~p->size ^ (uintptr_t)p;
}

void *malloc(size_t size)
{
    blk_hdr_t *p = mmap(0, size + sizeof(*p),
                        PROT_READ | PROT_WRITE,
                        0, -1, 0);

    p->size = size;
    set_sig(p);

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

void *realloc(void *p, size_t new_size)
{
    // Call malloc if the old pointer is null
    if (!p)
        return malloc(new_size);

    blk_hdr_t *h = (blk_hdr_t*)p - 1;

    // Do nothing if size didn't actually change
    if (new_size == h->size)
        return p;

    chk_sig(h);

    // Get old and new page count
    size_t old_pagecount = (h->size + (PAGE_SIZE - 1)) >> PAGE_SCALE;
    size_t new_pagecount = (new_size + (PAGE_SIZE - 1)) >> PAGE_SCALE;

    if (old_pagecount != new_pagecount) {
        // Size changed significantly

        // Remap pages
        p = mremap(h,
                   old_pagecount << PAGE_SCALE,
                   new_pagecount << PAGE_SCALE,
                   MREMAP_MAYMOVE);

        if (!p)
            return 0;

        h = (blk_hdr_t*)p - 1;
        h->size = new_size;
    } else {
        h->size = new_size;
    }
    set_sig(h);

    return p;
}

void free(void *p)
{
    blk_hdr_t *h = (blk_hdr_t*)p - 1;

    if (h->sig != (~h->size ^ (uintptr_t)p)) {
        munmap(h, h->size + sizeof(*h));
        return;
    }

    // Panic!
}
