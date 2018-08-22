#pragma once
#include "types.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"

template<typename Tkey = uintptr_t, typename Tval = uintptr_t>
class rbtree_t {
private:
    struct node_t;

    using key_t = Tkey;
    using val_t = Tval;

public:
    struct kvp_t {
        key_t key;
        val_t val;
    };

    typedef uint32_t iter_t;

    typedef int (*visitor_t)(kvp_t *kvp, void *p);
    typedef int (*cmp_t)(kvp_t const *lhs, kvp_t const *rhs, void *p);

    // Returns true if the tree is initialized
    operator bool() const;

    rbtree_t();
    ~rbtree_t();

    rbtree_t &init(cmp_t cmp, void *p);

    iter_t lower_bound_pair(kvp_t *kvp);
    iter_t lower_bound(key_t key, val_t val);

    void *upper_bound(void *key, iter_t *iter);

    iter_t insert_pair(kvp_t *kvp);

    iter_t insert(key_t key, val_t val);

    kvp_t *find(kvp_t *kvp, iter_t *iter);

    _pure iter_t first(iter_t start);
    _pure iter_t next(iter_t n);
    _pure iter_t prev(iter_t n);
    _pure iter_t last(iter_t start);

    _pure kvp_t item(iter_t iter);
    int delete_at(iter_t n);
    int delete_pair(kvp_t *kvp);
    int delete_item(key_t key, val_t val);

    iter_t item_count();
    int walk(visitor_t callback, void *p);
    int validate();

    static int test(void);

private:
    enum color_t : int {
        NOCOLOR,
        BLACK,
        RED
    };

    struct node_t {
        kvp_t kvp;

        iter_t parent;
        iter_t left;
        iter_t right;
        color_t color;
    };

    C_ASSERT(sizeof(node_t) == 32);

    void insert_case1(iter_t n);
    void insert_case2(iter_t n);
    void insert_case3(iter_t n);
    void insert_case4(iter_t n);
    void insert_case5(iter_t n);

    void delete_case6(iter_t n);
    void delete_case5(iter_t n);
    void delete_case4(iter_t n);
    void delete_case3(iter_t n);
    void delete_case2(iter_t n);
    void delete_case1(iter_t n);

    iter_t alloc_node();
    void free_node(iter_t n);
    void dump();
    iter_t grandparent(iter_t n);
    iter_t uncle(iter_t n);

    void replace_node(iter_t oldn, iter_t newn);

    void rotate_left(iter_t n);

    void rotate_right(iter_t n);

    iter_t new_node(kvp_t *kvp);

    int walk_impl(visitor_t callback, void *p, iter_t n);

    iter_t sibling(iter_t n);

    static int test_cmp(kvp_t const *lhs, kvp_t const *rhs, void *p);

    static int test_visit(kvp_t *kvp, void *p);

    node_t *nodes;

    // Comparator
    cmp_t cmp;
    void *cmp_param;

    // Size includes nil node at index 0
    iter_t size;

    // Amount of memory allocated in nodes,
    // including nil node at index 0
    iter_t capacity;

    iter_t root;
    iter_t free;

    // Number of allocated nodes
    iter_t count;

    uint32_t align[4];
};


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

#define RBTREE_CAPACITY_FROM_BYTES(n) \
    ((n > _MALLOC_OVERHEAD ? n - _MALLOC_OVERHEAD : 0) / sizeof(node_t))

#define RBTREE_PAGE_COUNT(c) \
    (((sizeof(node_t)*(c)) + PAGE_SIZE - 1) >> PAGE_SCALE)

#define RBTREE_NEXT_CAPACITY(c) \
    (c \
    ? RBTREE_CAPACITY_FROM_BYTES(RBTREE_PAGE_COUNT(c) * 2U * PAGE_SIZE) \
    : RBTREE_CAPACITY_FROM_BYTES(PAGE_SIZE - _MALLOC_OVERHEAD))

//
// Internals

#define NODE(i) (nodes + i)

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::alloc_node()
{
    iter_t n;

    if (free) {
        n = free;
        free = NODE(n)->right;
    } else if (size < capacity) {
        n = size++;
    } else {
        // Expand tree
        capacity = RBTREE_NEXT_CAPACITY(capacity);
        node_t *new_nodes = (node_t*)realloc(
                    nodes, sizeof(*nodes) * capacity);
        if (!new_nodes)
            return 0;


        if (unlikely(!nodes)) {
            memset(new_nodes, 0, sizeof(*nodes));
            new_nodes[0].color = BLACK;
        }

        nodes = new_nodes;

        n = size++;
    }

    ++count;

    return n;
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::free_node(iter_t n)
{
    --count;

    node_t *freed = NODE(n);

    freed->kvp.key = 0;
    freed->kvp.val = 0;
    freed->parent = 0;
    freed->left = 0;
    freed->right = free;
    freed->color = NOCOLOR;

    free = n;
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::dump()
{
    RBTREE_TRACE("ROOT=%u\n", root);
    for (iter_t i = 1; i < size; ++i)
        RBTREE_TRACE("%u) left=%u right=%u parent=%u c=%c item=%d\n",
               i, NODE(i)->left, NODE(i)->right,
               NODE(i)->parent,
               NODE(i)->color == RED ? 'R' : 'B',
               *(int*)NODE(i)->item);
    RBTREE_TRACE("---\n");
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::grandparent(iter_t n)
{
    if (n && NODE(n)->parent)
        return NODE(NODE(n)->parent)->parent;
    return 0;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::uncle(iter_t n)
{
    iter_t g = grandparent(n);
    if (g) {
        if (NODE(n)->parent == NODE(g)->left)
            return NODE(g)->right;
        else
            return NODE(g)->left;
    }
    return 0;
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::replace_node(iter_t oldn, iter_t newn)
{
    assert(oldn != 0);

    iter_t oldp = NODE(oldn)->parent;

    if (oldp == 0) {
        root = newn;
    } else {
        if (oldn == NODE(oldp)->left)
            NODE(oldp)->left = newn;
        else
            NODE(oldp)->right = newn;
    }

    if (newn)
        NODE(newn)->parent = oldp;
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::rotate_left(iter_t n)
{
    assert(n != 0);

    iter_t r = NODE(n)->right;

    replace_node(n, r);
    NODE(n)->right = NODE(r)->left;

    iter_t rl = NODE(r)->left;

    if (rl)
        NODE(rl)->parent = n;

    NODE(r)->left = n;
    NODE(n)->parent = r;
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::rotate_right(iter_t n)
{
    assert(n != 0);

    iter_t nl = NODE(n)->left;

    replace_node(n, nl);
    NODE(n)->left = NODE(nl)->right;

    iter_t lr = NODE(nl)->right;

    if (lr)
        NODE(lr)->parent = n;

    NODE(nl)->right = n;
    NODE(n)->parent = nl;
}

// Root can become black at any time, root must always be black
template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::insert_case1(iter_t n)
{
    RBTREE_TRACE("Insert case 1\n");

    if (!NODE(n)->parent)
        NODE(n)->color = BLACK;
    else
        insert_case2(n);
}

// n is not the root,
template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::insert_case2(iter_t n)
{
    RBTREE_TRACE("Insert case 2\n");

    if (NODE(NODE(n)->parent)->color != BLACK)
        insert_case3(n);
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::insert_case3(iter_t n)
{
    RBTREE_TRACE("Insert case 3\n");

    iter_t u = uncle(n);
    iter_t g;

    if (u && NODE(u)->color == RED) {
        NODE(NODE(n)->parent)->color = BLACK;
        NODE(u)->color = BLACK;
        g = grandparent(n);
        NODE(g)->color = RED;
        insert_case1(g);
    } else {
        insert_case4(n);
    }
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::insert_case4(iter_t n)
{
    RBTREE_TRACE("Insert case 4\n");

    iter_t g = grandparent(n);

    if ((n == NODE(NODE(n)->parent)->right &&
         (NODE(n)->parent == NODE(g)->left))) {
        rotate_left(NODE(n)->parent);
        n = NODE(n)->left;
    } else if ((n == NODE(NODE(n)->parent)->left) &&
               (NODE(n)->parent == NODE(g)->right)) {
        rotate_right(NODE(n)->parent);
        n = NODE(n)->right;
    }
    insert_case5(n);
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::insert_case5(iter_t n)
{
    RBTREE_TRACE("Insert case 5\n");

    iter_t g = grandparent(n);

    NODE(NODE(n)->parent)->color = BLACK;
    NODE(g)->color = RED;
    if (n == NODE(NODE(n)->parent)->left)
        rotate_right(g);
    else
        rotate_left(g);
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::new_node(kvp_t *kvp)
{
    iter_t p = 0;
    iter_t i = root;

    if (i) {
        int cmp_result;
        for (;;) {
            cmp_result = cmp(kvp, &NODE(i)->kvp, cmp_param);
            p = i;
            if (cmp_result < 0) {
                // item < node
                if (NODE(i)->left) {
                    i = NODE(i)->left;
                } else {
                    i = alloc_node();
                    NODE(p)->left = i;
                    break;
                }
            } else {
                // item >= node
                if (NODE(i)->right) {
                    i = NODE(i)->right;
                } else {
                    i = alloc_node();
                    NODE(p)->right = i;
                    break;
                }
            }
        }
    } else {
        // Tree was empty
        i = alloc_node();
        root = i;
    }
    NODE(i)->parent = p;
    NODE(i)->left = 0;
    NODE(i)->right = 0;
    NODE(i)->color = RED;
    NODE(i)->kvp = *kvp;
    return i;
}

// Returns first nonzero callback return value
// Returns 0 if it walked to the end of the tree
template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::walk_impl(visitor_t callback, void *p, iter_t n)
{
    int result;

    // Visit left subtree
    if (NODE(n)->left) {
        assert(NODE(NODE(n)->left)->parent == n);
        result = walk_impl(callback,
                                  p, NODE(n)->left);
        if (result)
            return result;
    }

    // Visit this node
    result = callback(&NODE(n)->kvp, p);
    if (result)
        return result;

    // Visit right subtree
    if (NODE(n)->right) {
        assert(NODE(NODE(n)->right)->parent == n);
        result = walk_impl(callback,
                                  p, NODE(n)->right);
        if (result)
            return result;
    }

    return result;
}

//
// Public API

template<typename Tkey, typename Tval>
rbtree_t<Tkey,Tval>::rbtree_t()
    : nodes(nullptr)
    , cmp(nullptr)
    , cmp_param(nullptr)
    , size(0)
    , capacity(0)
    , root(0)
    , free(0)
    , count(0)
{
}

template<typename Tkey, typename Tval>
rbtree_t<Tkey,Tval>::operator bool() const
{
    return nodes != nullptr;
}

template<typename Tkey, typename Tval>
rbtree_t<Tkey,Tval> &
rbtree_t<Tkey,Tval>::init(cmp_t init_cmp, void *p)
{
    cmp = init_cmp;
    cmp_param = p;
    root = 0;
    free = 0;

    // Size includes nil node
    size = 1;
    capacity = 0;
    count = 0;

    nodes = nullptr;

    // Cause initial allocation
    free_node(alloc_node());

    return *this;
}

template<typename Tkey, typename Tval>
rbtree_t<Tkey,Tval>::~rbtree_t()
{
    // FIXME: call destructors
    ::free(nodes);
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::lower_bound_pair(kvp_t *kvp)
{
    iter_t iter = 0;
    find(kvp, &iter);
    return iter;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::lower_bound(key_t key, val_t val)
{
    kvp_t kvp = { key, val };
    return lower_bound_pair(&kvp);
}

//template<typename Tkey, typename Tval>
//void *rbtree_t<Tkey,Tval>::upper_bound(void *item, iter_t *iter)
//{
//
//}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::insert_pair(kvp_t *kvp)
{
    iter_t i = new_node(kvp);
    insert_case1(i);
    //dump();
    return i;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::insert(key_t key, val_t val)
{
    kvp_t kvp = { key, val };
    return insert_pair(&kvp);
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::next(iter_t n)
{
    if (!n)
        return 0;

    if (NODE(n)->right) {
        // Find lowest value in right subtree
        n = NODE(n)->right;
        while (NODE(n)->left)
            n = NODE(n)->left;

        return n;
    }

    iter_t p = NODE(n)->parent;
    while (p && n == NODE(p)->right) {
        n = p;
        p = NODE(n)->parent;
    }
    return p;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::prev(iter_t n)
{
    if (!n)
        return 0;

    if (NODE(n)->left) {
        // Find highest value in right subtree
        n = NODE(n)->left;
        while (NODE(n)->right)
            n = NODE(n)->right;

        return n;
    }

    iter_t p = NODE(n)->parent;
    while (p && n == NODE(p)->left) {
        n = p;
        p = NODE(n)->parent;
    }
    return p;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::kvp_t
rbtree_t<Tkey,Tval>::item(iter_t iter)
{
    return nodes[iter].kvp;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::first(iter_t start)
{
    iter_t i = start ? start : root;

    if (i) {
        while (NODE(i)->left)
            i = NODE(i)->left;

        return i;
    }

    return 0;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::last(iter_t start)
{
    iter_t i = start ? start : root;

    if (i) {
        while (NODE(i)->right)
            i = NODE(i)->right;

        return i;
    }

    return 0;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::kvp_t *
rbtree_t<Tkey,Tval>::find(kvp_t *kvp, iter_t *iter)
{
    iter_t n = root;
    iter_t next;
    int cmp_result = -1;

    for (; n; n = next) {
        node_t const *node = NODE(n);

        cmp_result = cmp(kvp, &node->kvp, cmp_param);

        if (cmp_result == 0)
            break;

        if (cmp_result < 0)
            next = node->left;
        else
            next = node->right;

        if (!next)
            break;
    }

    if (iter)
        *iter = n;
    else
        *iter = 0;

    return cmp_result ? nullptr : &NODE(n)->kvp;
}

//
// Delete

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::sibling(iter_t n)
{
    assert(n);

    iter_t parent = NODE(n)->parent;

    assert(parent);

    if (NODE(parent)->left == n)
        return NODE(parent)->right;
    return NODE(parent)->left;
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::delete_case6(iter_t n)
{
    iter_t nparent = NODE(n)->parent;
    iter_t nsib = sibling(n);

    NODE(nsib)->color = NODE(nparent)->color;
    NODE(nparent)->color = BLACK;
    if (n == NODE(nparent)->left) {
        assert(NODE(NODE(nsib)->right)->color == RED);
        NODE(NODE(nsib)->right)->color = BLACK;
        rotate_left(nparent);
    } else {
        assert(NODE(NODE(nsib)->left)->color == RED);
        NODE(NODE(nsib)->left)->color = BLACK;
        rotate_right(nparent);
    }
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::delete_case5(iter_t n)
{
    iter_t nparent = NODE(n)->parent;
    iter_t nsib = sibling(n);

    if (n == NODE(nparent)->left &&
            NODE(nsib)->color == BLACK &&
            NODE(NODE(nsib)->left)->color == RED &&
            NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        NODE(NODE(nsib)->left)->color = BLACK;
        rotate_right(nsib);
    } else if (n == NODE(nparent)->right &&
               NODE(nsib)->color == BLACK &&
               NODE(NODE(nsib)->right)->color == RED &&
               NODE(NODE(nsib)->left)->color == BLACK) {
        NODE(nsib)->color = RED;
        NODE(NODE(nsib)->right)->color = BLACK;
        rotate_left(nsib);
    }
    delete_case6(n);
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::delete_case4(iter_t n)
{
    iter_t nparent = NODE(n)->parent;
    iter_t nsib = sibling(n);

    if (NODE(nparent)->color == RED &&
            NODE(nsib)->color == BLACK &&
            NODE(NODE(nsib)->left)->color == BLACK &&
            NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        NODE(nparent)->color = BLACK;
    } else {
        delete_case5(n);
    }
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::delete_case3(iter_t n)
{
    iter_t nparent = NODE(n)->parent;
    iter_t nsib = sibling(n);

    if (NODE(nparent)->color == BLACK &&
            NODE(nsib)->color == BLACK &&
            NODE(NODE(nsib)->left)->color == BLACK &&
            NODE(NODE(nsib)->right)->color == BLACK) {
        NODE(nsib)->color = RED;
        delete_case1(nparent);
    } else {
        delete_case4(n);
    }
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::delete_case2(iter_t n)
{
    iter_t nsib = sibling(n);

    if (NODE(nsib)->color == RED) {
        iter_t nparent = NODE(n)->parent;
        NODE(nparent)->color = RED;
        NODE(nsib)->color = BLACK;
        if (n == NODE(nparent)->left)
            rotate_left(nparent);
        else
            rotate_right(nparent);
    }
    delete_case3(n);
}

template<typename Tkey, typename Tval>
void rbtree_t<Tkey,Tval>::delete_case1(iter_t n)
{
    if (NODE(n)->parent)
        delete_case2(n);
}

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::delete_at(iter_t n)
{
    iter_t child;
    iter_t left;
    iter_t right;

    left = NODE(n)->left;
    right = NODE(n)->right;

    if (left && right) {
        // Find highest value in left subtree
        iter_t pred = last(left);

        // Move the highest node in the left child
        // to this node and delete that node
        NODE(n)->kvp.key = NODE(pred)->kvp.key;
        NODE(n)->kvp.val = NODE(pred)->kvp.val;
        n = pred;
        left = NODE(n)->left;
        right = NODE(n)->right;
    }

    assert(!left || !right);
    child = !right ? left : right;
    if (NODE(n)->color == BLACK) {
        NODE(n)->color = NODE(child)->color;
        delete_case1(n);
    }
    replace_node(n, child);
    if (!NODE(n)->parent && child)
        NODE(child)->color = BLACK;
    free_node(n);

    return 1;
}

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::delete_pair(kvp_t *kvp)
{
    iter_t n;
    if (find(kvp, &n))
        return delete_at(n);
    return 0;
}

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::delete_item(key_t key, val_t val)
{
    kvp_t kvp = { key, val };
    return delete_pair(&kvp);
}

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::walk(visitor_t callback, void *p)
{
    if (root != 0)
        return walk_impl(callback, p, root);

    return 0;
}

template<typename Tkey, typename Tval>
typename rbtree_t<Tkey,Tval>::iter_t
rbtree_t<Tkey,Tval>::item_count()
{
    return size - 1;
}

//
// Test

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::test_cmp(kvp_t const *lhs, kvp_t const *rhs, void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            lhs->val < rhs->val ? -1 :
            rhs->val < lhs->val ? -1 :
            0;
}

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::test_visit(kvp_t *kvp, void *p)
{
    (void)p;
    (void)kvp;
    RBTREE_TRACE("Item: key=%d val=%d\n", kvp->key, kvp->val);
    return 0;
}

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::validate()
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

    if (NODE(0)->kvp.key != 0) {
        assert(!"Nil node has key");
        return 0;
    }

    if (NODE(0)->kvp.val != 0) {
        assert(!"Nil node has val");
        return 0;
    }

    if (NODE(0)->color != BLACK) {
        assert(!"Nil node is not black");
        return 0;
    }

    for (size_t i = 1; i < size; ++i) {
        int left = NODE(i)->left;
        int right = NODE(i)->right;

        if (left) {
            if (NODE(left)->parent != i) {
                assert(!"Left child parent link is incorrect");
                return 0;
            }

            if (cmp(&NODE(left)->kvp,
                          &NODE(i)->kvp,
                          cmp_param) >= 0) {
                assert(!"Left child is >= its parent");
                return 0;
            }
        }

        if (right) {
            if (NODE(right)->parent != i) {
                assert(!"Right child parent link is incorrect");
                return 0;
            }

            if (cmp(&NODE(right)->kvp,
                          &NODE(i)->kvp, cmp_param) < 0) {
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

template<typename Tkey, typename Tval>
int rbtree_t<Tkey,Tval>::test(void)
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
            rbtree_t tree;
            tree.init(&rbtree_t::test_cmp, nullptr);

            scenario[0] = orders[order][(0 + pass) & 3];
            scenario[1] = orders[order][(1 + pass) & 3];
            scenario[2] = orders[order][(2 + pass) & 3];
            scenario[3] = orders[order][(3 + pass) & 3];

            RBTREE_TRACE("Trying %2d %2d %2d %2d\n",
                   values[scenario[0]],
                   values[scenario[1]],
                   values[scenario[2]],
                   values[scenario[3]]);

            tree.insert(values[scenario[0]], 0);
            tree.insert(values[scenario[1]], 0);
            tree.insert(values[scenario[2]], 0);
            tree.insert(values[scenario[3]], 0);

            tree.walk(test_visit, nullptr);

            RBTREE_TRACE("---\n");

            for (int del = 0; del < 4; ++del) {
                RBTREE_TRACE("Delete %d\n", values[scenario[del]]);
                tree.delete_item(values[scenario[del]], 0);
                //tree.dump();
                tree.walk(test_visit, nullptr);
                RBTREE_TRACE("---\n");
            }
        }
    }

    int seq[24];
    for (int dist = 4; dist <= 24; ++dist) {
        for (int pass = 0; pass < 2; ++pass) {
            rbtree_t tree;
            tree.init(&rbtree_t::test_cmp, nullptr);

            for (int i = 0; i < dist; ++i) {
                if (!pass)
                    seq[i] = i + 3;
                else
                    seq[i] = 27 - i;

                tree.insert(seq[i], 0);
                tree.validate();
            }

            tree.walk(test_visit, nullptr);

            RBTREE_TRACE("---\n");
        }
    }
    return 0;
}
