#pragma once
#include "types.h"

struct heap_t;

// Fast heap

heap_t *heap_create(void);
void heap_destroy(heap_t *heap);

_malloc _assume_aligned(16) _alloc_size(2, 3)
void *heap_calloc(heap_t *heap, size_t num, size_t size);

_malloc _assume_aligned(16) _alloc_size(2)
void *heap_alloc(heap_t *heap, size_t size);

void heap_free(heap_t *heap, void *block);

_assume_aligned(16) _alloc_size(3)
void *heap_realloc(heap_t *heap, void *block, size_t size);

// Page heap

_malloc _assume_aligned(16) _alloc_size(2)
void *pageheap_calloc(size_t num, size_t size);

_malloc _assume_aligned(16) _alloc_size(1)
void *pageheap_alloc(size_t size);
void pageheap_free(void *block);

_assume_aligned(16) _alloc_size(2)
void *pageheap_realloc(void *block, size_t size);
