#pragma once
#include "types.h"

typedef struct rbtree_t rbtree_t;
typedef uint32_t rbtree_iter_t;

typedef int (*rbtree_visitor_t)(rbtree_t *tree, void *item, void *p);

typedef int (*rbtree_cmp_t)(void *a, void *b, void *p);

rbtree_t *rbtree_create(rbtree_cmp_t cmp, void *p, size_t capacity);
void rbtree_destroy(rbtree_t *tree);
void *rbtree_lower_bound(void *key, rbtree_iter_t *iter);
void *rbtree_upper_bound(void *key, rbtree_iter_t *iter);
void *rbtree_insert(rbtree_t *tree, void *item, rbtree_iter_t *iter);
void *rbtree_first(rbtree_t *tree, rbtree_iter_t *iter);
void *rbtree_next(rbtree_t *tree, rbtree_iter_t *iter);
void *rbtree_prev(rbtree_t *tree, rbtree_iter_t *iter);
void *rbtree_last(rbtree_t *tree, rbtree_iter_t *iter);
void *rbtree_item(rbtree_t *tree, rbtree_iter_t iter);
void rbtree_delete(rbtree_t *tree, void *item);

rbtree_iter_t rbtree_count(rbtree_t *tree);
int rbtree_walk(rbtree_t *tree, rbtree_visitor_t callback, void *p);
int rbtree_validate(rbtree_t *tree);

int rbtree_test(void);
