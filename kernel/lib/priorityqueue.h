#pragma once
#include "types.h"

struct priqueue_t;
typedef int (*priqueue_comparator_t)(uintptr_t lhs, uintptr_t rhs, void *ctx);
typedef void (*priqueue_swapped_t)(uintptr_t a, uintptr_t b, void *ctx);

priqueue_t *priqueue_create(uint32_t capacity,
        priqueue_comparator_t cmp, priqueue_swapped_t swapped, void *ctx);
void priqueue_destroy(priqueue_t *queue);

uintptr_t priqueue_peek(priqueue_t *queue);
void priqueue_push(priqueue_t *queue, uintptr_t item);
uintptr_t priqueue_pop(priqueue_t *queue);

size_t priqueue_update(priqueue_t *queue, size_t index);
void priqueue_delete(priqueue_t *queue, size_t index);

size_t priqueue_count(priqueue_t *queue);
