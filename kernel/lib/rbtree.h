#pragma once
#include "types.h"

typedef struct rbtree_t rbtree_t;
typedef uint32_t rbtree_iter_t;

typedef uintptr_t rbtree_key_t;
typedef uintptr_t rbtree_val_t;

typedef struct rbtree_kvp_t {
    rbtree_key_t key;
    rbtree_val_t val;
} rbtree_kvp_t;

typedef int (*rbtree_visitor_t)(rbtree_t *tree,
                                rbtree_kvp_t *kvp,
                                void *p);

typedef int (*rbtree_cmp_t)(
        rbtree_kvp_t *lhs,
        rbtree_kvp_t *rhs,
        void *p);

rbtree_t *rbtree_create(rbtree_cmp_t cmp, void *p, size_t capacity);
void rbtree_destroy(rbtree_t *tree);

rbtree_iter_t rbtree_lower_bound_pair(rbtree_t *tree,
                                 rbtree_kvp_t *kvp);
rbtree_iter_t rbtree_lower_bound(rbtree_t *tree,
                                      rbtree_key_t key,
                                      rbtree_val_t val);

void *rbtree_upper_bound(rbtree_t *tree,
                         void *key,
                         rbtree_iter_t *iter);

rbtree_iter_t rbtree_insert_pair(rbtree_t *tree,
                            rbtree_kvp_t *kvp);

rbtree_iter_t rbtree_insert(rbtree_t *tree,
                            rbtree_key_t key,
                            rbtree_val_t val);

rbtree_kvp_t *rbtree_find(rbtree_t *tree,
                         rbtree_kvp_t *kvp,
                         rbtree_iter_t *iter);

rbtree_iter_t rbtree_first(rbtree_t *tree, rbtree_iter_t start);
rbtree_iter_t rbtree_next(rbtree_t *tree, rbtree_iter_t n);
rbtree_iter_t rbtree_prev(rbtree_t *tree, rbtree_iter_t n);
rbtree_iter_t rbtree_last(rbtree_t *tree, rbtree_iter_t start);

rbtree_kvp_t rbtree_item(rbtree_t *tree, rbtree_iter_t iter);
int rbtree_delete_at(rbtree_t *tree, rbtree_iter_t n);
int rbtree_delete_pair(rbtree_t *tree,
                       rbtree_kvp_t *kvp);
int rbtree_delete(rbtree_t *tree,
                  rbtree_key_t key,
                  rbtree_val_t val);

rbtree_iter_t rbtree_count(rbtree_t *tree);
int rbtree_walk(rbtree_t *tree, rbtree_visitor_t callback, void *p);
int rbtree_validate(rbtree_t *tree);

int rbtree_test(void);
