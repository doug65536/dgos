#include "rbtree.h"
#include "stdlib.h"
#include "string.h"
#include "likely.h"
#include "assert.h"


// Red-Black Tree Properties
//  0. The root is black
//  1. New nodes are red
//  2. A red node has two black children
//  3. Nil nodes are black
//  4. Every path from the root to a nil node
//     has the same number of black nodes

#define RBTREE_TRACE_ON    0
#if RBTREE_TRACE_ON
#include "printk.h"
#define RBTREE_TRACE(...) printk (__VA_ARGS__)
#else
#define RBTREE_TRACE(...) ((void)0)
#endif

typedef enum rbtree_color_t {
    NOCOLOR,
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
    RBTREE_TRACE("ROOT=%u\n", tree->root);
    for (rbtree_iter_t i = 1; i < tree->size; ++i)
        RBTREE_TRACE("%u) left=%u right=%u parent=%u c=%c item=%d\n",
               i, NODE(i)->left, NODE(i)->right,
               NODE(i)->parent,
               NODE(i)->color == RED ? 'R' : 'B',
               *(int*)NODE(i)->item);
    RBTREE_TRACE("---\n");
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

static void rbtree_insert_case1(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case2(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case3(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case4(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_insert_case5(rbtree_t *tree, rbtree_iter_t n);

static void rbtree_replace_node(
        rbtree_t *tree, rbtree_iter_t oldn,
        rbtree_iter_t newn)
{
    assert(oldn != 0);

    rbtree_iter_t oldp = NODE(oldn)->parent;

    if (oldp == 0) {
        tree->root = newn;
    } else {
        if (oldn == NODE(oldp)->left)
            NODE(oldp)->left = newn;
        else
            NODE(oldp)->right = newn;
    }

    if (newn)
        NODE(newn)->parent = oldp;
}

static void rbtree_rotate_left(
        rbtree_t *tree, rbtree_iter_t n)
{
    assert(n != 0);

    rbtree_iter_t r = NODE(n)->right;

    rbtree_replace_node(tree, n, r);
    NODE(n)->right = NODE(r)->left;

    rbtree_iter_t rl = NODE(r)->left;

    if (rl)
        NODE(rl)->parent = n;

    NODE(r)->left = n;
    NODE(n)->parent = r;
}

static void rbtree_rotate_right(
        rbtree_t *tree, rbtree_iter_t n)
{
    assert(n != 0);

    rbtree_iter_t nl = NODE(n)->left;

    rbtree_replace_node(tree, n, nl);
    NODE(n)->left = NODE(nl)->right;

    rbtree_iter_t lr = NODE(nl)->right;

    if (lr)
        NODE(lr)->parent = n;

    NODE(nl)->right = n;
    NODE(n)->parent = nl;
}

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
        rbtree_rotate_left(tree, NODE(n)->parent);
        n = NODE(n)->left;
    } else if ((n == NODE(NODE(n)->parent)->left) &&
               (NODE(n)->parent == NODE(g)->right)) {
        rbtree_rotate_right(tree, NODE(n)->parent);
        n = NODE(n)->right;
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
        assert(NODE(NODE(n)->left)->parent == n);
        result = rbtree_walk_impl(tree, callback,
                                  p, NODE(n)->left);
        if (result)
            return result;
    }

    // Visit this node
    result = callback(tree, NODE(n)->item, p);
    if (result)
        return result;

    // Visit right subtree
    if (NODE(n)->right) {
        assert(NODE(NODE(n)->right)->parent == n);
        result = rbtree_walk_impl(tree, callback,
                                  p, NODE(n)->right);
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
        tree->nodes[0].color = BLACK;
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
    rbtree_iter_t i = iter ? *iter : tree->root;

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

static void *rbtree_find(rbtree_t *tree, void *key,
                  rbtree_iter_t *iter)
{
    rbtree_iter_t n = tree->root;
    rbtree_iter_t next;
    int cmp = -1;

    for (n = tree->root; n; n = next) {
        cmp = tree->cmp(key, NODE(n)->item, tree->p);

        if (cmp == 0)
            break;

        if (cmp < 0)
            next = NODE(n)->left;
        else
            next = NODE(n)->right;

        if (!next)
            break;
    }

    if (iter)
        *iter = n;

    return cmp ? 0 : NODE(n)->item;
}

//
// Delete

static rbtree_iter_t rbtree_sibling(rbtree_t *tree, rbtree_iter_t n)
{
    assert(n);

    rbtree_iter_t parent = NODE(n)->parent;

    assert(parent);

    if (NODE(parent)->left == n)
        return NODE(parent)->right;
    return NODE(parent)->left;
}

static void rbtree_delete_case6(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_delete_case5(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_delete_case4(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_delete_case3(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_delete_case2(rbtree_t *tree, rbtree_iter_t n);
static void rbtree_delete_case1(rbtree_t *tree, rbtree_iter_t n);

static void rbtree_delete_case6(rbtree_t *tree, rbtree_iter_t n)
{
    rbtree_iter_t nparent = NODE(n)->parent;
    rbtree_iter_t nsib = rbtree_sibling(tree, n);

    NODE(n)->color = NODE(nparent)->color;
    NODE(nparent)->color = BLACK;
    if (n == NODE(nparent)->left) {
        assert(NODE(nsib)->right == RED);
        NODE(NODE(nsib)->right)->color = BLACK;
        rbtree_rotate_left(tree, nparent);
    } else {
        assert(NODE(nsib)->left == RED);
        NODE(NODE(nsib)->left)->color = BLACK;
        rbtree_rotate_left(tree, nparent);
    }
}

static void rbtree_delete_case5(rbtree_t *tree, rbtree_iter_t n)
{
    rbtree_iter_t nparent = NODE(n)->parent;
    rbtree_iter_t nsib = rbtree_sibling(tree, n);

    if (n == NODE(nparent)->left &&
            NODE(nsib)->color == BLACK &&
            NODE(NODE(nsib)->left)->color == RED &&
            NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        NODE(NODE(nsib)->left)->color = BLACK;
        rbtree_rotate_right(tree, nsib);
    } else if (n == NODE(nparent)->left &&
               NODE(nsib)->color == BLACK &&
               NODE(NODE(nsib)->left)->color == RED &&
               NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        NODE(NODE(nsib)->right)->color = BLACK;
        rbtree_rotate_left(tree, nsib);
    }
    rbtree_delete_case6(tree, n);
}

static void rbtree_delete_case4(rbtree_t *tree, rbtree_iter_t n)
{
    rbtree_iter_t nparent = NODE(n)->parent;
    rbtree_iter_t nsib = rbtree_sibling(tree, n);

    if (NODE(nparent)->color == RED &&
            NODE(nsib)->color == BLACK &&
            NODE(NODE(nsib)->left)->color == BLACK &&
            NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        NODE(nparent)->color = BLACK;
    } else {
        rbtree_delete_case5(tree, n);
    }
}

static void rbtree_delete_case3(rbtree_t *tree, rbtree_iter_t n)
{
    rbtree_iter_t nparent = NODE(n)->parent;
    rbtree_iter_t nsib = rbtree_sibling(tree, n);

    if (NODE(nparent)->color == BLACK &&
            NODE(nsib)->color == BLACK &&
            NODE(NODE(nsib)->left)->color == BLACK &&
            NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        rbtree_delete_case1(tree, nparent);
    } else {
        rbtree_delete_case4(tree, n);
    }
}

static void rbtree_delete_case2(rbtree_t *tree, rbtree_iter_t n)
{
    rbtree_iter_t sib = rbtree_sibling(tree, n);

    if (NODE(sib)->color == RED) {
        rbtree_iter_t nparent = NODE(n)->parent;
        NODE(nparent)->color = RED;
        NODE(sib)->color = BLACK;
        if (n == NODE(nparent)->left)
            rbtree_rotate_left(tree, nparent);
        else
            rbtree_rotate_right(tree, nparent);
    }
    rbtree_delete_case3(tree, n);
}

static void rbtree_delete_case1(rbtree_t *tree, rbtree_iter_t n)
{
    if (NODE(n)->parent)
        rbtree_delete_case2(tree, n);
}

static void rbtree_free_node(rbtree_t *tree, rbtree_iter_t n)
{
    // TODO: implement free node
    (void)tree;
    (void)n;
}

void rbtree_delete(rbtree_t *tree, void *item)
{
    rbtree_iter_t child;
    rbtree_iter_t n;
    rbtree_iter_t left;
    rbtree_iter_t right;

    if (!rbtree_find(tree, item, &n))
        return;

    left = NODE(n)->left;
    right = NODE(n)->right;

    if (left && right) {
        // Find highest value in left subtree
        rbtree_iter_t pred = left;
        rbtree_last(tree, &pred);

        // Move the highest node in the left child
        // to this node and delete that node
        NODE(n)->item = NODE(pred)->item;
        n = pred;
        left = NODE(n)->left;
        right = NODE(n)->right;
    }

    assert(!left || !right);
    child = !right ? left : right;
    if (NODE(n)->color == BLACK) {
        NODE(n)->color = NODE(child)->color;
        rbtree_delete_case1(tree, n);
    }
    rbtree_replace_node(tree, n, child);
    if (!NODE(n)->parent && child)
        NODE(child)->color = BLACK;
    rbtree_free_node(tree, n);
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
    (void)item;
    RBTREE_TRACE("Item: %d\n", *(int*)item);
    return 0;
}

int rbtree_validate(rbtree_t *tree)
{
    if (NODE(0)->left != 0) {
        assert(!"Nil node has left child");
        return 0;
    }

    if (NODE(0)->parent != 0) {
        assert(!"Nil node has parent");
        return 0;
    }

    if (NODE(0)->right != 0) {
        assert(!"Nil node has right child");
        return 0;
    }

    if (NODE(0)->item != 0) {
        assert(!"Nil node has item");
        return 0;
    }

    if (NODE(0)->color != BLACK) {
        assert(!"Nil node is not black");
        return 0;
    }

    for (size_t i = 1; i < tree->size; ++i) {
        int left = NODE(i)->left;
        int right = NODE(i)->right;

        if (left) {
            if (NODE(left)->parent != i) {
                assert(!"Left child parent link is incorrect");
                return 0;
            }

            if (tree->cmp(NODE(left)->item, NODE(i)->item, tree->p) >= 0) {
                assert(!"Left child is >= its parent");
                return 0;
            }
        }

        if (right) {
            if (NODE(right)->parent != i) {
                assert(!"Right child parent link is incorrect");
                return 0;
            }

            if (tree->cmp(NODE(right)->item, NODE(i)->item, tree->p) < 0) {
                assert(!"Right child is < its parent");
                return 0;
            }
        }

        if (NODE(i)->color != BLACK && NODE(i)->color != RED) {
            assert(!"Node has invalid color");
            return 0;
        }
    }

    return 1;
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

            RBTREE_TRACE("Trying %2d %2d %2d %2d\n",
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

            RBTREE_TRACE("---\n");
        }
    }

    int seq[24];
    for (int dist = 4; dist <= 24; ++dist) {
        for (int pass = 0; pass < 2; ++pass) {
            rbtree_t *tree = rbtree_create(rbtree_test_cmp, 0, 0);

            for (int i = 0; i < dist; ++i) {
                if (!pass)
                    seq[i] = i + 3;
                else
                    seq[i] = 27 - i;

                rbtree_insert(tree, seq + i, 0);
                rbtree_validate(tree);
            }

            rbtree_walk(tree, rbtree_test_visit, 0);

            rbtree_destroy(tree);

            RBTREE_TRACE("---\n");
        }
    }
    return 0;
}
