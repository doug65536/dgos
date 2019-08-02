#pragma once
#include "types.h"

struct heap_t;

// Fast heap

uint32_t heap_get_heap_id(heap_t *heap);
uint32_t heap_get_block_heap_id(void *block);

heap_t *heap_create(void);
void heap_destroy(heap_t *heap);

_malloc _assume_aligned(16) _alloc_size(2, 3)
void *heap_calloc(heap_t *heap, size_t num, size_t size);

_malloc _assume_aligned(16) _alloc_size(2)
void *heap_alloc(heap_t *heap, size_t size);

void heap_free(heap_t *heap, void *block);



_assume_aligned(16) _alloc_size(3)
void *heap_realloc(heap_t *heap, void *block, size_t size);

bool heap_maybe_blk(heap_t *heap, void *block);

bool heap_validate(heap_t *heap, bool dump = false);

// Page heap

heap_t *pageheap_create();
void pageheap_destroy(heap_t *heap);

_malloc _assume_aligned(16) _alloc_size(2)
void *pageheap_calloc(heap_t *heap, size_t num, size_t size);

_malloc _assume_aligned(16) _alloc_size(2)
void *pageheap_alloc(heap_t *heap, size_t size);
void pageheap_free(heap_t *heap, void *block);

_assume_aligned(16) _alloc_size(3)
void *pageheap_realloc(heap_t *heap, void *block, size_t size);
