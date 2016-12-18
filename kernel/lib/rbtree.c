#include "rbtree.h"
#include "stdlib.h"
#include "string.h"
#include "likely.h"
#include "assert.h"

#include "printk.h"

// Red-Black Tree Properties
//  0. The root is black
//  1. New nodes are red
//  2. A red node has two black children
//  3. Nil nodes are black
//  4. Every path from the root to a nil node
//     has the same number of black nodes

#define RBTREE_TRACE_ON    1
#if RBTREE_TRACE_ON
#define RBTREE_TRACE(...) printk (__VA_ARGS__)
#else
#define RBTREE_TRACE(...) ((void)0)
#endif

typedef enum rbtree_color_t {
    BLACK,
    RED
} rbtree_color_t;

typedef struct rbtree_node_t rbtree_node_t;
struct rbtree_node_t {
    void *item;
    rbtree_iter_t parent;
    rbtree_iter_t left;
    rbtree_iter_t right;
    rbtree_color_t color;
};

struct rbtree_t {
    // Comparator
    int (*cmp)(void *a, void *b, void *p);
    void *p;

    rbtree_iter_t root;

    rbtree_node_t *nodes;

    // Size includes nil node at index 0
    size_t size;

    size_t capacity;
};

#define RBTREE_CAPACITY_FROM_BYTES(n) \
    (((n)-sizeof(rbtree_t)) / sizeof(rbtree_node_t))

#define RBTREE_PAGE_COUNT(c) \
    (((sizeof(rbtree_t) + sizeof(rbtree_node_t)*(c)) + \
    PAGE_SIZE - 1) / PAGE_SIZE)

#define RBTREE_NEXT_CAPACITY(c) \
    RBTREE_CAPACITY_FROM_BYTES(RBTREE_PAGE_COUNT(c) * \
    2U * PAGE_SIZE)

//
// Internals

#define NODE(i) (tree->nodes + i)

static void rbtree_dump(rbtree_t *tree)
{
    printk("ROOT=%u\n", tree->root);
    for (rbtree_iter_t i = 1; i < tree->size; ++i)
        printk("%u) left=%u right=%u parent=%u c=%c item=%d\n",
               i, NODE(i)->left, NODE(i)->right,
               NODE(i)->parent,
               NODE(i)->color == RED ? 'R' : 'B',
               *(int*)NODE(i)->item);
    printk("---\n");
}

static rbtree_iter_t rbtree_grandparent(
        rbtree_t *tree, rbtree_iter_t n)
{
    if (n && NODE(n)->parent)
        return NODE(NODE(n)->parent)->parent;
    return 0;
}

static rbtree_iter_t rbtree_uncle(
        rbtree_t *tree, rbtree_iter_t n)
{
    rbtree_iter_t g = rbtree_grandparent(tree, n);
    if (g) {
        if (NODE(n)->parent == NODE(g)->left)
            return NODE(g)->right;
        else
            return NODE(g)->left;
    }
    return 0;
}

//static int rbtree_is_root(
//        rbtree_t *tree, rbtree_iter_t n)
//{
//    return n && NODE(n)->parent == 0;
//}
//
//static int rbtree_is_left_child(
//        rbtree_t *tree, rbtree_iter_t n)
//{
//    return !rbtree_is_root(tree, n) &&
//            NODE(NODE(n)->parent)->left == n;
//}
//
//static int rbtree_is_right_child(
//        rbtree_t *tree, rbtree_iter_t n)
//{
//    return !rbtree_is_root(tree, n) &&
//            NODE(NODE(n)->parent)->right == n;
//}

static void rbtree_insert_case1(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case2(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case3(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case4(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case5(rbtree_t *tree, rbtree_iter_t n);

// Rotate p left,
// n is right child of p
// n replaces p,
// p becomes left child of n
// left child of n becomes right child of p
static rbtree_iter_t rbtree_rotate_left(
        rbtree_t *tree, rbtree_iter_t p)
{
    assert(p != 0);

    rbtree_iter_t g = NODE(p)->parent;

    //assert(g != 0);

    RBTREE_TRACE("Rotating left n=%u g=%u\n", p, g);

    rbtree_iter_t n = NODE(p)->right;
    rbtree_iter_t nl = NODE(n)->left;
    if (g) {
        NODE(g)->left = n;
        NODE(n)->parent = g;
    } else {
        tree->root = n;
        NODE(n)->parent = 0;
    }
    NODE(p)->right = nl;
    NODE(nl)->parent = p;

    NODE(n)->left = p;
    NODE(p)->parent = n;

    p = NODE(n)->left;
    return p;
}

static rbtree_iter_t rbtree_rotate_right(
        rbtree_t *tree, rbtree_iter_t p)
{
    assert(p != 0);

    rbtree_iter_t g = NODE(p)->parent;

    //assert(g != 0);

    RBTREE_TRACE("Rotating right n=%u g=%u\n", p, g);

    rbtree_iter_t n = NODE(p)->left;
    rbtree_iter_t nr = NODE(n)->right;
    if (g) {
        NODE(g)->right = n;
        NODE(n)->parent = g;
    } else {
        tree->root = n;
        NODE(n)->parent = 0;
    }
    NODE(p)->left = nr;
    NODE(nr)->parent = p;

    NODE(n)->right = p;
    NODE(p)->parent = n;

    p = NODE(n)->right;
    return p;
}

//static rbtree_iter_t rbtree_rotate_left(
//        rbtree_t *tree, rbtree_iter_t c, rbtree_iter_t g)
//{
//    RBTREE_TRACE("Rotating left\n");

//    rbtree_iter_t sp = NODE(g)->left;
//    rbtree_iter_t sl = NODE(c)->left;
//    NODE(g)->left = c;
//    NODE(c)->left = sp;
//    NODE(sp)->right = sl;
//    c = NODE(c)->left;
//    return c;
//}

//static rbtree_iter_t rbtree_rotate_right(
//        rbtree_t *tree, rbtree_iter_t c, rbtree_iter_t g)
//{
//    RBTREE_TRACE("Rotating right\n");

//    rbtree_iter_t sp = NODE(g)->right;
//    rbtree_iter_t sr = NODE(c)->right;
//    NODE(g)->right = sp;
//    NODE(sp)->left = sr;
//    c = NODE(c)->right;
//    return c;
//}

// Root can become black at any time, root must always be black
static void rbtree_insert_case1(rbtree_t *tree, rbtree_iter_t n)
{
    RBTREE_TRACE("Insert case 1\n");

    if (!NODE(n)->parent)
        NODE(n)->color = BLACK;
    else
        rbtree_insert_case2(tree, n);
}

// n is not the root,
static void rbtree_insert_case2(rbtree_t *tree, rbtree_iter_t n)
{
    RBTREE_TRACE("Insert case 2\n");

    if (NODE(NODE(n)->parent)->color != BLACK)
        rbtree_insert_case3(tree, n);
}

static void rbtree_insert_case3(rbtree_t *tree, rbtree_iter_t n)
{
    RBTREE_TRACE("Insert case 3\n");

    rbtree_iter_t u = rbtree_uncle(tree, n);
    rbtree_iter_t g;

    if (u && NODE(u)->color == RED) {
        NODE(NODE(n)->parent)->color = BLACK;
        NODE(u)->color = BLACK;
        g = rbtree_grandparent(tree, n);
        NODE(g)->color = RED;
        rbtree_insert_case1(tree, g);
    } else {
        rbtree_insert_case4(tree, n);
    }
}

static void rbtree_insert_case4(rbtree_t *tree, rbtree_iter_t n)
{
    RBTREE_TRACE("Insert case 4\n");

    rbtree_iter_t g = rbtree_grandparent(tree, n);

    if ((n == NODE(NODE(n)->parent)->right &&
         (NODE(n)->parent == NODE(g)->left))) {
        n = rbtree_rotate_left(tree, NODE(n)->parent);
    } else if ((n == NODE(NODE(n)->parent)->left) &&
               (NODE(n)->parent == NODE(g)->right)) {
        n = rbtree_rotate_right(tree, NODE(n)->parent);
    }
    rbtree_insert_case5(tree, n);
}

static void rbtree_insert_case5(rbtree_t *tree, rbtree_iter_t n)
{
    RBTREE_TRACE("Insert case 5\n");

    rbtree_iter_t g = rbtree_grandparent(tree, n);

    NODE(NODE(n)->parent)->color = BLACK;
    NODE(g)->color = RED;
    if (n == NODE(NODE(n)->parent)->left)
        rbtree_rotate_right(tree, g);
    else
        rbtree_rotate_left(tree, g);
}

static rbtree_iter_t rbtree_new_node(rbtree_t *tree, void *item)
{
    rbtree_iter_t p = 0;
    rbtree_iter_t i = tree->root;

    if (i) {
        int cmp;
        for (;;) {
            cmp = tree->cmp(item, NODE(i)->item, tree->p);
            p = i;
            if (cmp < 0) {
                // item < node
                if (NODE(i)->left) {
                    i = NODE(i)->left;
                } else {
                    i = tree->size++;
                    NODE(p)->left = i;
                    break;
                }
            } else {
                // item >= node
                if (NODE(i)->right) {
                    i = NODE(i)->right;
                } else {
                    i = tree->size++;
                    NODE(p)->right = i;
                    break;
                }
            }
        }
    } else {
        i = tree->size++;
        tree->root = i;
    }
    NODE(i)->parent = p;
    NODE(i)->left = 0;
    NODE(i)->right = 0;
    NODE(i)->color = RED;
    NODE(i)->item = item;
    return i;
}

// Returns first nonzero callback return value
// Returns 0 if it walked to the end of the tree
static int rbtree_walk_impl(rbtree_t *tree,
                     rbtree_visitor_t callback,
                     void *p,
                     rbtree_iter_t n)
{
    int result;

    // Visit left subtree
    if (NODE(n)->left) {
        result = rbtree_walk_impl(tree, callback, p, NODE(n)->left);
        if (result)
            return result;
    }

    // Visit this node
    result = callback(tree, NODE(n)->item, p);
    if (result)
        return result;

    // Visit right subtree
    if (NODE(n)->right) {
        result = rbtree_walk_impl(tree, callback, p, NODE(n)->right);
        if (result)
            return result;
    }

    return result;
}

//
// Public API

rbtree_t *rbtree_create(rbtree_cmp_t cmp,
                        void *p,
                        size_t capacity)
{
    if (capacity == 0)
        capacity = RBTREE_CAPACITY_FROM_BYTES(PAGE_SIZE);

    rbtree_t *tree = malloc(sizeof(*tree) +
                            sizeof(*tree->nodes) *
                            capacity);

    if (tree) {
        tree->cmp = cmp;
        tree->p = p;
        tree->root = 0;

        // Size includes nil node
        tree->size = 1;
        tree->capacity = capacity;

        tree->nodes = (rbtree_node_t*)(tree + 1);

        // Clear nil node
        memset(tree->nodes, 0, sizeof(*tree->nodes));
    }

    return tree;
}

void rbtree_destroy(rbtree_t *tree)
{
    free(tree);
}

//void *rbtree_lower_bound(void *item, rbtree_iter_t *iter)
//{
//
//}
//
//void *rbtree_upper_bound(void *item, rbtree_iter_t *iter)
//{
//
//}

void *rbtree_insert(rbtree_t *tree,
                    void *item,
                    rbtree_iter_t *iter)
{
    rbtree_iter_t i = rbtree_new_node(tree, item);
    rbtree_insert_case1(tree, i);
    if (iter)
        *iter = i;
    rbtree_dump(tree);
    return NODE(i)->item;
}

//void *rbtree_next(rbtree_t *tree, rbtree_iter_t *iter)
//{
//    rbtree_iter_t i = *iter;
//
//    if (NODE(i)->right) {
//        // Find lowest value in right subtree
//        i = NODE(i)->right;
//        while (NODE(i)->left)
//            i = NODE(i)->left;
//        if (iter)
//            *iter = i;
//        return NODE(i)->item;
//    } else {
//        i = NODE(i)->parent;
//    }
//}
//
//void *rbtree_prev(rbtree_t *tree, rbtree_iter_t *iter)
//{
//
//}

void *rbtree_item(rbtree_t *tree, rbtree_iter_t iter)
{
    rbtree_iter_t i = iter;
    return i ? tree->nodes[i].item : 0;
}

void *rbtree_first(rbtree_t *tree, rbtree_iter_t *iter)
{
    rbtree_iter_t i = tree->root;

    if (i) {
        while (NODE(i)->left)
            i = NODE(i)->left;

        if (iter)
            *iter = i;

        return NODE(i)->item;
    }

    if (iter)
        *iter = 0;

    return 0;
}

void *rbtree_last(rbtree_t *tree, rbtree_iter_t *iter)
{
    rbtree_iter_t i = tree->root;

    if (i) {
        while (NODE(i)->right)
            i = NODE(i)->right;

        if (iter)
            *iter = i;

        return NODE(i)->item;
    }

    if (iter)
        *iter = 0;

    return 0;
}

int rbtree_walk(rbtree_t *tree,
                     rbtree_visitor_t callback,
                     void *p)
{
    if (tree->root != 0)
        return rbtree_walk_impl(tree, callback, p, tree->root);

    return 0;
}

rbtree_iter_t rbtree_count(rbtree_t *tree)
{
    return tree->size - 1;
}

//
// Test

static int rbtree_test_cmp(void *lhs, void *rhs, void *p)
{
    (void)p;
    int *a = lhs;
    int *b = rhs;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

static int rbtree_test_visit(rbtree_t *tree, void *item, void *p)
{
    (void)tree;
    (void)p;
    int *a = item;
    printk("Item: %d\n", *a);
    return 0;
}

int rbtree_test(void)
{
    static int values[4] = {
        42, 21,  7, 16
    };

    static int orders[][4] = {
        { 0, 1, 2, 3 },
        { 0, 1, 3, 2 },
        { 0, 3, 1, 2 },
        { 0, 3, 2, 1 }
    };

    int scenario[4];

    for (int pass = 0; pass < 4; ++pass) {
        for (int order = 0; order < 4; ++order) {
            rbtree_t *tree = rbtree_create(rbtree_test_cmp, 0, 0);

            scenario[0] = orders[order][(0 + pass) & 3];
            scenario[1] = orders[order][(1 + pass) & 3];
            scenario[2] = orders[order][(2 + pass) & 3];
            scenario[3] = orders[order][(3 + pass) & 3];

            printk("Trying %2d %2d %2d %2d\n",
                   values[scenario[0]],
                   values[scenario[1]],
                   values[scenario[2]],
                   values[scenario[3]]);

            rbtree_insert(tree, values + scenario[0], 0);
            rbtree_insert(tree, values + scenario[1], 0);
            rbtree_insert(tree, values + scenario[2], 0);
            rbtree_insert(tree, values + scenario[3], 0);

            rbtree_walk(tree, rbtree_test_visit, 0);

            rbtree_destroy(tree);

            printk("---\n");
        }
    }

    return 0;
}
