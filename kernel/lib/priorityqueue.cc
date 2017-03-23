#include "priorityqueue.h"

#include "likely.h"
#include "assert.h"
#include "string.h"
#include "mm.h"

struct priqueue_t {
    uint32_t capacity;
    uint32_t count;
    priqueue_comparator_t cmp;
    priqueue_swapped_t swapped;
    void *ctx;
    void *align[4];
};

C_ASSERT(sizeof(priqueue_t) >= 64);
C_ASSERT_ISPO2(sizeof(priqueue_t));

static inline uintptr_t *priqueue_item(priqueue_t *queue, size_t index)
{
    return (uintptr_t*)(queue+1) + index;
}

priqueue_t *priqueue_create(uint32_t capacity,
                            priqueue_comparator_t cmp,
                            priqueue_swapped_t swapped,
                            void *ctx)
{
    if (capacity == 0)
        capacity = (PAGESIZE - sizeof(priqueue_t)) / sizeof(uintptr_t);

	priqueue_t *queue = (priqueue_t*)mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                             MAP_POPULATE, -1, 0);

    queue->capacity = capacity;
    queue->count = 0;
    queue->cmp = cmp;
    queue->swapped = swapped;
    queue->ctx = ctx;
    memset(queue->align, 0, sizeof(queue->align));

    return queue;
}

void priqueue_destroy(priqueue_t *queue)
{
	char *end = (char*)priqueue_item(queue, queue->capacity);
	char *begin = (char*)queue;
    size_t size = end - begin;
    munmap(queue, size);
}

static int priqueue_grow(priqueue_t *queue)
{
    // not implemented (yet)
    (void)queue;
    return 0;
}

static inline size_t priqueue_leftchild(size_t index)
{
    return (index + index) + 1;
}

static inline size_t priqueue_parent(size_t index)
{
    return (index - 1) >> 1;
}

static inline size_t priqueue_swap(priqueue_t *queue, size_t a, size_t b)
{
    uintptr_t *ap = priqueue_item(queue, a);
    uintptr_t *bp = priqueue_item(queue, b);

    uintptr_t tmp = *ap;
    *ap = *bp;
    *bp = tmp;

    if (queue->swapped)
        queue->swapped(*ap, *bp, queue->ctx);

    return a;
}

static size_t priqueue_siftup(priqueue_t *queue, size_t index)
{
    while (index != 0) {
        size_t parent = priqueue_parent(index);
        int cmp = queue->cmp(parent, index, queue->ctx);
        if (cmp >= 0)
            break;
        index = priqueue_swap(queue, parent, index);
    }
    return index;
}

static size_t priqueue_siftdown(priqueue_t *queue, size_t index)
{
    assert(queue->count > 0);

    for (;;) {
        size_t child = priqueue_leftchild(index);
        if (child < queue->count) {
            uintptr_t l_val = *priqueue_item(queue, child);
            uintptr_t r_val = *priqueue_item(queue, child + 1);
            int cmp = queue->cmp(l_val, r_val, queue->ctx);
            // Swap with right child if right child is smaller
            child += (cmp > 0);
            uintptr_t this_item = *priqueue_item(queue, index);
            uintptr_t child_item = *priqueue_item(queue, child);
            cmp = queue->cmp(this_item, child_item, queue->ctx);
            if (cmp > 0) {
                index = priqueue_swap(queue, child, index);
                continue;
            }
        }
        return index;
    }
}

uintptr_t priqueue_peek(priqueue_t *queue)
{
    assert(queue->count > 0);
    return *priqueue_item(queue, 0);
}

void priqueue_push(priqueue_t *queue, uintptr_t item)
{
    if (unlikely(queue->count >= queue->capacity))
        priqueue_grow(queue);

    size_t index = queue->count;
    *priqueue_item(queue, index) = item;
    priqueue_siftup(queue, index);
}

uintptr_t priqueue_pop(priqueue_t *queue)
{
    assert(queue->count > 0);

    uintptr_t *top = priqueue_item(queue, 0);
    uintptr_t *last = priqueue_item(queue, queue->count - 1);
    uintptr_t item = *top;
    *top = *last;
    --queue->count;
    priqueue_siftdown(queue, 0);
    return item;
}

size_t priqueue_update(priqueue_t *queue, size_t index)
{
    if (assert(index < queue->count)) {
        index = priqueue_siftup(queue, index);
        index = priqueue_siftdown(queue, index);
    }
    return index;
}

void priqueue_delete(priqueue_t *queue, size_t index)
{
    if (assert(index < queue->count)) {
        uintptr_t *top = priqueue_item(queue, index);
        uintptr_t *last = priqueue_item(queue, queue->count - 1);
        *top = *last;
        --queue->count;
        priqueue_update(queue, index);
    }
}

size_t priqueue_count(priqueue_t *queue)
{
    return queue->count;
}
