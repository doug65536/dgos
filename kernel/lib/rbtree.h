#pragma once
#include "types.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"

template<typename _Tkey = uintptr_t, typename _Tval = uintptr_t>
class rbtree_t {
private:
    struct __node_t;

    template<typename _U>
    static int default_cmp(_U const& __lhs, _U const& __rhs, void*)
    {
        if (__lhs.key < __rhs.key)
            return -1;

        if (__rhs.key < __lhs.key)
            return 1;

        return 0;
    }

public:
    using key_type = _Tkey;
    using value_type = _Tval;

    struct kvp_t {
        key_type key;
        value_type val;
    };

    using iter_t = uint32_t;

    typedef int (*visitor_t)(kvp_t *__kvp, void *__p);
    typedef int (*cmp_t)(kvp_t const *__lhs, kvp_t const *__rhs, void *__p);

    // Returns true if the tree is initialized
    operator bool() const;

    rbtree_t();
    ~rbtree_t();

    rbtree_t &init();

    rbtree_t &init(cmp_t __cmp, void *__p);

    size_t size() const
    {
        return __count;
    }

    size_t capacity() const
    {
        return __capacity;
    }

    iter_t lower_bound_pair(kvp_t *__kvp);
    iter_t lower_bound(_Tkey __key, _Tval __val);

    iter_t insert_pair(kvp_t *__kvp);

    iter_t insert(_Tkey __key, _Tval __val);

    kvp_t *find(kvp_t *__kvp, iter_t *__iter);

    _pure iter_t first(iter_t __start);
    _pure iter_t next(iter_t __n);
    _pure iter_t prev(iter_t __n);
    _pure iter_t last(iter_t __start);

    _pure kvp_t& item(iter_t __iter);
    int delete_at(iter_t __n);
    int delete_pair(kvp_t *__kvp);
    int delete_item(_Tkey __key, _Tval __val);

    iter_t item_count();
    int walk(visitor_t __callback, void *__p);
    int validate();

    static int test(void);

private:
    enum __color_t : int {
        _NOCOLOR,
        _BLACK,
        _RED
    };

    struct __node_t {
        kvp_t __kvp;

        iter_t __parent;
        iter_t __left;
        iter_t __right;
        __color_t __color;
    };

    C_ASSERT(sizeof(__node_t) == 32);

    void insert_case1(iter_t __n);
    void insert_case2(iter_t __n);
    void insert_case3(iter_t __n);
    void insert_case4(iter_t __n);
    void insert_case5(iter_t __n);

    void delete_case6(iter_t __n);
    void delete_case5(iter_t __n);
    void delete_case4(iter_t __n);
    void delete_case3(iter_t __n);
    void delete_case2(iter_t __n);
    void delete_case1(iter_t __n);

    iter_t alloc_node();
    void free_node(iter_t __n);
    void dump();
    iter_t grandparent(iter_t __n);
    iter_t uncle(iter_t __n);

    void replace_node(iter_t __oldn, iter_t __newn);

    void rotate_left(iter_t __n);

    void rotate_right(iter_t __n);

    iter_t new_node(kvp_t *__kvp);

    int walk_impl(visitor_t __callback, void *__p, iter_t __n);

    iter_t sibling(iter_t __n);

    static int test_cmp(kvp_t const *__lhs, kvp_t const *__rhs, void *__p);

    static int test_visit(kvp_t *__kvp, void *__p);

    __node_t *__nodes;

    // Comparator
    cmp_t __cmp;
    void *__cmp_param;

    // Size includes nil node at index 0
    iter_t __size;

    // Amount of memory allocated in nodes,
    // including nil node at index 0
    iter_t __capacity;

    iter_t __root;
    iter_t __free;

    // Number of allocated nodes
    iter_t __count;

    uint32_t __align[4];
};


// Red-Black Tree Properties
//  0. The root is black
//  1. New nodes are red
//  2. A red node has two black children
//  3. Nil nodes are black
//  4. Every path from the root to a nil node
//     has the same number of black nodes

#define _RBTREE_TRACE_ON    0
#if _RBTREE_TRACE_ON
#include "printk.h"
#define _RBTREE_TRACE(...) printk (__VA_ARGS__)
#else
#define _RBTREE_TRACE(...) ((void)0)
#endif

#define _RBTREE_CAPACITY_FROM_BYTES(n) \
    ((n > _MALLOC_OVERHEAD ? n - _MALLOC_OVERHEAD : 0) / sizeof(__node_t))

#define _RBTREE_PAGE_COUNT(c) \
    (((sizeof(__node_t)*(c)) + PAGE_SIZE - 1) >> PAGE_SCALE)

#define _RBTREE_NEXT_CAPACITY(c) \
    (c \
    ? _RBTREE_CAPACITY_FROM_BYTES(_RBTREE_PAGE_COUNT(c) * 2U * PAGE_SIZE) \
    : _RBTREE_CAPACITY_FROM_BYTES(PAGE_SIZE - _MALLOC_OVERHEAD))

//
// Internals

#define _NODE(i) (__nodes + i)

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::alloc_node()
{
    iter_t __n;

    if (__free) {
        __n = __free;
        __free = _NODE(__n)->__right;
    } else if (__size < __capacity) {
        __n = __size++;
    } else {
        // Expand tree
        __capacity = _RBTREE_NEXT_CAPACITY(__capacity);
        __node_t *__new_nodes = (__node_t*)realloc(
                    __nodes, sizeof(*__nodes) * __capacity);
        if (!__new_nodes)
            return 0;


        if (unlikely(!__nodes)) {
            memset(__new_nodes, 0, sizeof(*__nodes));
            __new_nodes[0].__color = _BLACK;
        }

        __nodes = __new_nodes;

        __n = __size++;
    }

    ++__count;

    return __n;
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::free_node(iter_t __n)
{
    --__count;

    __node_t *__freed = _NODE(__n);

    __freed->__kvp.key = 0;
    __freed->__kvp.val = 0;
    __freed->__parent = 0;
    __freed->__left = 0;
    __freed->__right = __free;
    __freed->__color = _NOCOLOR;

    __free = __n;
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::dump()
{
    _RBTREE_TRACE("ROOT=%u\n", __root);
    for (iter_t i = 1; i < __size; ++i)
        _RBTREE_TRACE("%u) left=%u right=%u parent=%u c=%c item=%d\n",
               i, _NODE(i)->left, _NODE(i)->right,
               _NODE(i)->parent,
               _NODE(i)->color == _RED ? 'R' : 'B',
               *(int*)_NODE(i)->item);
    _RBTREE_TRACE("---\n");
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::grandparent(iter_t __n)
{
    if (__n && _NODE(__n)->__parent)
        return _NODE(_NODE(__n)->__parent)->__parent;
    return 0;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::uncle(iter_t __n)
{
    iter_t g = grandparent(__n);
    if (g) {
        if (_NODE(__n)->__parent == _NODE(g)->__left)
            return _NODE(g)->__right;
        else
            return _NODE(g)->__left;
    }
    return 0;
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::replace_node(iter_t __oldn, iter_t __newn)
{
    assert(__oldn != 0);

    iter_t oldp = _NODE(__oldn)->__parent;

    if (oldp == 0) {
        __root = __newn;
    } else {
        if (__oldn == _NODE(oldp)->__left)
            _NODE(oldp)->__left = __newn;
        else
            _NODE(oldp)->__right = __newn;
    }

    if (__newn)
        _NODE(__newn)->__parent = oldp;
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::rotate_left(iter_t __n)
{
    assert(__n != 0);

    iter_t r = _NODE(__n)->__right;

    replace_node(__n, r);
    _NODE(__n)->__right = _NODE(r)->__left;

    iter_t rl = _NODE(r)->__left;

    if (rl)
        _NODE(rl)->__parent = __n;

    _NODE(r)->__left = __n;
    _NODE(__n)->__parent = r;
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::rotate_right(iter_t __n)
{
    assert(__n != 0);

    iter_t nl = _NODE(__n)->__left;

    replace_node(__n, nl);
    _NODE(__n)->__left = _NODE(nl)->__right;

    iter_t lr = _NODE(nl)->__right;

    if (lr)
        _NODE(lr)->__parent = __n;

    _NODE(nl)->__right = __n;
    _NODE(__n)->__parent = nl;
}

// Root can become black at any time, root must always be black
template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::insert_case1(iter_t __n)
{
    _RBTREE_TRACE("Insert case 1\n");

    if (!_NODE(__n)->__parent)
        _NODE(__n)->__color = _BLACK;
    else
        insert_case2(__n);
}

// n is not the root,
template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::insert_case2(iter_t __n)
{
    _RBTREE_TRACE("Insert case 2\n");

    if (_NODE(_NODE(__n)->__parent)->__color != _BLACK)
        insert_case3(__n);
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::insert_case3(iter_t __n)
{
    _RBTREE_TRACE("Insert case 3\n");

    iter_t u = uncle(__n);
    iter_t g;

    if (u && _NODE(u)->__color == _RED) {
        _NODE(_NODE(__n)->__parent)->__color = _BLACK;
        _NODE(u)->__color = _BLACK;
        g = grandparent(__n);
        _NODE(g)->__color = _RED;
        insert_case1(g);
    } else {
        insert_case4(__n);
    }
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::insert_case4(iter_t __n)
{
    _RBTREE_TRACE("Insert case 4\n");

    iter_t g = grandparent(__n);

    if ((__n == _NODE(_NODE(__n)->__parent)->__right &&
         (_NODE(__n)->__parent == _NODE(g)->__left))) {
        rotate_left(_NODE(__n)->__parent);
        __n = _NODE(__n)->__left;
    } else if ((__n == _NODE(_NODE(__n)->__parent)->__left) &&
               (_NODE(__n)->__parent == _NODE(g)->__right)) {
        rotate_right(_NODE(__n)->__parent);
        __n = _NODE(__n)->__right;
    }
    insert_case5(__n);
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::insert_case5(iter_t __n)
{
    _RBTREE_TRACE("Insert case 5\n");

    iter_t g = grandparent(__n);

    _NODE(_NODE(__n)->__parent)->__color = _BLACK;
    _NODE(g)->__color = _RED;
    if (__n == _NODE(_NODE(__n)->__parent)->__left)
        rotate_right(g);
    else
        rotate_left(g);
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::new_node(kvp_t *__kvp)
{
    iter_t p = 0;
    iter_t i = __root;

    if (i) {
        int cmp_result;
        for (;;) {
            cmp_result = __cmp(__kvp, &_NODE(i)->__kvp, __cmp_param);
            p = i;
            if (cmp_result < 0) {
                // item < node
                if (_NODE(i)->__left) {
                    i = _NODE(i)->__left;
                } else {
                    i = alloc_node();
                    _NODE(p)->__left = i;
                    break;
                }
            } else {
                // item >= node
                if (_NODE(i)->__right) {
                    i = _NODE(i)->__right;
                } else {
                    i = alloc_node();
                    _NODE(p)->__right = i;
                    break;
                }
            }
        }
    } else {
        // Tree was empty
        i = alloc_node();
        __root = i;
    }
    _NODE(i)->__parent = p;
    _NODE(i)->__left = 0;
    _NODE(i)->__right = 0;
    _NODE(i)->__color = _RED;
    _NODE(i)->__kvp = *__kvp;
    return i;
}

// Returns first nonzero callback return value
// Returns 0 if it walked to the end of the tree
template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::walk_impl(
        visitor_t __callback, void *__p, iter_t __n)
{
    int result;

    // Visit left subtree
    if (_NODE(__n)->__left) {
        assert(_NODE(_NODE(__n)->__left)->__parent == __n);
        result = walk_impl(__callback,
                           __p, _NODE(__n)->__left);
        if (result)
            return result;
    }

    // Visit this node
    result = __callback(&_NODE(__n)->__kvp, __p);
    if (result)
        return result;

    // Visit right subtree
    if (_NODE(__n)->__right) {
        assert(_NODE(_NODE(__n)->__right)->__parent == __n);
        result = walk_impl(__callback,
                           __p, _NODE(__n)->__right);
        if (result)
            return result;
    }

    return result;
}

//
// Public API

template<typename _Tkey, typename _Tval>
rbtree_t<_Tkey,_Tval>::rbtree_t()
    : __nodes(nullptr)
    , __cmp(nullptr)
    , __cmp_param(nullptr)
    , __size(0)
    , __capacity(0)
    , __root(0)
    , __free(0)
    , __count(0)
{
}

template<typename _Tkey, typename _Tval>
rbtree_t<_Tkey,_Tval>::operator bool() const
{
    return __nodes != nullptr;
}

template<typename _Tkey, typename _Tval>
rbtree_t<_Tkey,_Tval> &
rbtree_t<_Tkey,_Tval>::init()
{
    return init(default_cmp<_Tkey>, nullptr);
}

template<typename _Tkey, typename _Tval>
rbtree_t<_Tkey,_Tval> &
rbtree_t<_Tkey,_Tval>::init(cmp_t init_cmp, void *p)
{
    if (init_cmp) {
        __cmp = init_cmp;
        __cmp_param = p;
    } else {
    }
    __root = 0;
    __free = 0;

    // Size includes nil node
    __size = 1;
    __capacity = 0;
    __count = 0;

    __nodes = nullptr;

    // Cause initial allocation
    free_node(alloc_node());

    return *this;
}

template<typename _Tkey, typename _Tval>
rbtree_t<_Tkey,_Tval>::~rbtree_t()
{
    // FIXME: call destructors
    ::free(__nodes);
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::lower_bound_pair(kvp_t *__kvp)
{
    iter_t iter = 0;
    find(__kvp, &iter);
    return iter;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::lower_bound(_Tkey __key, _Tval __val)
{
    kvp_t kvp = { __key, __val };
    return lower_bound_pair(&kvp);
}

//template<typename _Tkey, typename _Tval>
//void *rbtree_t<_Tkey,_Tval>::upper_bound(void *item, iter_t *iter)
//{
//
//}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::insert_pair(kvp_t *__kvp)
{
    iter_t i = new_node(__kvp);
    insert_case1(i);
    //dump();
    return i;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::insert(_Tkey __key, _Tval __val)
{
    kvp_t kvp = { __key, __val };
    return insert_pair(&kvp);
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::next(iter_t __n)
{
    if (!__n)
        return 0;

    if (_NODE(__n)->__right) {
        // Find lowest value in right subtree
        __n = _NODE(__n)->__right;
        while (_NODE(__n)->__left)
            __n = _NODE(__n)->__left;

        return __n;
    }

    iter_t __p = _NODE(__n)->__parent;
    while (__p && __n == _NODE(__p)->__right) {
        __n = __p;
        __p = _NODE(__n)->__parent;
    }
    return __p;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::prev(iter_t __n)
{
    if (!__n)
        return 0;

    if (_NODE(__n)->__left) {
        // Find highest value in right subtree
        __n = _NODE(__n)->__left;
        while (_NODE(__n)->__right)
            __n = _NODE(__n)->__right;

        return __n;
    }

    iter_t p = _NODE(__n)->__parent;
    while (p && __n == _NODE(p)->__left) {
        __n = p;
        p = _NODE(__n)->__parent;
    }
    return p;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::kvp_t&
rbtree_t<_Tkey,_Tval>::item(iter_t __iter)
{
    return __nodes[__iter].__kvp;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::first(iter_t __start)
{
    iter_t i = __start ? __start : __root;

    if (i) {
        while (_NODE(i)->__left)
            i = _NODE(i)->__left;

        return i;
    }

    return 0;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::last(iter_t __start)
{
    iter_t i = __start ? __start : __root;

    if (i) {
        while (_NODE(i)->__right)
            i = _NODE(i)->__right;

        return i;
    }

    return 0;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::kvp_t *
rbtree_t<_Tkey,_Tval>::find(kvp_t *__kvp, iter_t *__iter)
{
    iter_t n = __root;
    iter_t next;
    int cmp_result = -1;

    for (; n; n = next) {
        __node_t const *node = _NODE(n);

        cmp_result = __cmp(__kvp, &node->__kvp, __cmp_param);

        if (cmp_result == 0)
            break;

        if (cmp_result < 0)
            next = node->__left;
        else
            next = node->__right;

        if (!next)
            break;
    }

    if (__iter)
        *__iter = n;
    else
        *__iter = 0;

    return cmp_result ? nullptr : &_NODE(n)->__kvp;
}

//
// Delete

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::sibling(iter_t __n)
{
    assert(__n);

    iter_t parent = _NODE(__n)->__parent;

    assert(parent);

    if (_NODE(parent)->__left == __n)
        return _NODE(parent)->__right;
    return _NODE(parent)->__left;
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::delete_case6(iter_t __n)
{
    iter_t nparent = _NODE(__n)->__parent;
    iter_t nsib = sibling(__n);

    _NODE(nsib)->__color = _NODE(nparent)->__color;
    _NODE(nparent)->__color = _BLACK;
    if (__n == _NODE(nparent)->__left) {
        assert(_NODE(_NODE(nsib)->__right)->__color == _RED);
        _NODE(_NODE(nsib)->__right)->__color = _BLACK;
        rotate_left(nparent);
    } else {
        assert(_NODE(_NODE(nsib)->__left)->__color == _RED);
        _NODE(_NODE(nsib)->__left)->__color = _BLACK;
        rotate_right(nparent);
    }
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::delete_case5(iter_t __n)
{
    iter_t nparent = _NODE(__n)->__parent;
    iter_t nsib = sibling(__n);

    if (__n == _NODE(nparent)->__left &&
            _NODE(nsib)->__color == _BLACK &&
            _NODE(_NODE(nsib)->__left)->__color == _RED &&
            _NODE(_NODE(nsib)->__right)->__color == _BLACK) {
        _NODE(nsib)->__color = _RED;
        _NODE(_NODE(nsib)->__left)->__color = _BLACK;
        rotate_right(nsib);
    } else if (__n == _NODE(nparent)->__right &&
               _NODE(nsib)->__color == _BLACK &&
               _NODE(_NODE(nsib)->__right)->__color == _RED &&
               _NODE(_NODE(nsib)->__left)->__color == _BLACK) {
        _NODE(nsib)->__color = _RED;
        _NODE(_NODE(nsib)->__right)->__color = _BLACK;
        rotate_left(nsib);
    }
    delete_case6(__n);
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::delete_case4(iter_t __n)
{
    iter_t nparent = _NODE(__n)->__parent;
    iter_t nsib = sibling(__n);

    if (_NODE(nparent)->__color == _RED &&
            _NODE(nsib)->__color == _BLACK &&
            _NODE(_NODE(nsib)->__left)->__color == _BLACK &&
            _NODE(_NODE(nsib)->__right)->__color == _BLACK) {
        _NODE(nsib)->__color = _RED;
        _NODE(nparent)->__color = _BLACK;
    } else {
        delete_case5(__n);
    }
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::delete_case3(iter_t __n)
{
    iter_t nparent = _NODE(__n)->__parent;
    iter_t nsib = sibling(__n);

    if (_NODE(nparent)->__color == _BLACK &&
            _NODE(nsib)->__color == _BLACK &&
            _NODE(_NODE(nsib)->__left)->__color == _BLACK &&
            _NODE(_NODE(nsib)->__right)->__color == _BLACK) {
        _NODE(nsib)->__color = _RED;
        delete_case1(nparent);
    } else {
        delete_case4(__n);
    }
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::delete_case2(iter_t __n)
{
    iter_t nsib = sibling(__n);

    if (_NODE(nsib)->__color == _RED) {
        iter_t nparent = _NODE(__n)->__parent;
        _NODE(nparent)->__color = _RED;
        _NODE(nsib)->__color = _BLACK;
        if (__n == _NODE(nparent)->__left)
            rotate_left(nparent);
        else
            rotate_right(nparent);
    }
    delete_case3(__n);
}

template<typename _Tkey, typename _Tval>
void rbtree_t<_Tkey,_Tval>::delete_case1(iter_t __n)
{
    if (_NODE(__n)->__parent)
        delete_case2(__n);
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::delete_at(iter_t __n)
{
    iter_t child;
    iter_t left;
    iter_t right;

    left = _NODE(__n)->__left;
    right = _NODE(__n)->__right;

    if (left && right) {
        // Find highest value in left subtree
        iter_t pred = last(left);

        // Move the highest node in the left child
        // to this node and delete that node
        _NODE(__n)->__kvp.key = _NODE(pred)->__kvp.key;
        _NODE(__n)->__kvp.val = _NODE(pred)->__kvp.val;
        __n = pred;
        left = _NODE(__n)->__left;
        right = _NODE(__n)->__right;
    }

    assert(!left || !right);
    child = !right ? left : right;
    if (_NODE(__n)->__color == _BLACK) {
        _NODE(__n)->__color = _NODE(child)->__color;
        delete_case1(__n);
    }
    replace_node(__n, child);
    if (!_NODE(__n)->__parent && child)
        _NODE(child)->__color = _BLACK;
    free_node(__n);

    return 1;
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::delete_pair(kvp_t *__kvp)
{
    iter_t __n;
    if (find(__kvp, &__n))
        return delete_at(__n);
    return 0;
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::delete_item(_Tkey __key, _Tval __val)
{
    kvp_t kvp = { __key, __val };
    return delete_pair(&kvp);
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::walk(visitor_t __callback, void *__p)
{
    if (__root != 0)
        return walk_impl(__callback, __p, __root);

    return 0;
}

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::iter_t
rbtree_t<_Tkey,_Tval>::item_count()
{
    return __size - 1;
}

//
// Test

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::test_cmp(
        kvp_t const *__lhs, kvp_t const *__rhs, void *__p)
{
    (void)__p;
    return __lhs->key < __rhs->key ? -1 :
            __rhs->key < __lhs->key ? 1 :
            __lhs->val < __rhs->val ? -1 :
            __rhs->val < __lhs->val ? -1 :
            0;
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::test_visit(kvp_t *__kvp, void *__p)
{
    (void)__p;
    (void)__kvp;
    _RBTREE_TRACE("Item: key=%d val=%d\n", __kvp->key, __kvp->val);
    return 0;
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::validate()
{
    if (_NODE(0)->__left != 0) {
        assert(!"Nil node has left child");
        return 0;
    }

    if (_NODE(0)->__parent != 0) {
        assert(!"Nil node has parent");
        return 0;
    }

    if (_NODE(0)->__right != 0) {
        assert(!"Nil node has right child");
        return 0;
    }

    if (_NODE(0)->__kvp.key != 0) {
        assert(!"Nil node has key");
        return 0;
    }

    if (_NODE(0)->__kvp.val != 0) {
        assert(!"Nil node has val");
        return 0;
    }

    if (_NODE(0)->__color != _BLACK) {
        assert(!"Nil node is not black");
        return 0;
    }

    for (size_t i = 1; i < __size; ++i) {
        int left = _NODE(i)->__left;
        int right = _NODE(i)->__right;

        if (left) {
            if (_NODE(left)->__parent != i) {
                assert(!"Left child parent link is incorrect");
                return 0;
            }

            if (__cmp(&_NODE(left)->__kvp,
                          &_NODE(i)->__kvp,
                          __cmp_param) >= 0) {
                assert(!"Left child is >= its parent");
                return 0;
            }
        }

        if (right) {
            if (_NODE(right)->__parent != i) {
                assert(!"Right child parent link is incorrect");
                return 0;
            }

            if (__cmp(&_NODE(right)->__kvp,
                          &_NODE(i)->__kvp, __cmp_param) < 0) {
                assert(!"Right child is < its parent");
                return 0;
            }
        }

        if (_NODE(i)->__color != _BLACK && _NODE(i)->__color != _RED) {
            assert(!"Node has invalid color");
            return 0;
        }
    }

    return 1;
}

template<typename _Tkey, typename _Tval>
int rbtree_t<_Tkey,_Tval>::test(void)
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

            _RBTREE_TRACE("Trying %2d %2d %2d %2d\n",
                   values[scenario[0]],
                   values[scenario[1]],
                   values[scenario[2]],
                   values[scenario[3]]);

            tree.insert(values[scenario[0]], 0);
            tree.insert(values[scenario[1]], 0);
            tree.insert(values[scenario[2]], 0);
            tree.insert(values[scenario[3]], 0);

            tree.walk(test_visit, nullptr);

            _RBTREE_TRACE("---\n");

            for (int del = 0; del < 4; ++del) {
                _RBTREE_TRACE("Delete %d\n", values[scenario[del]]);
                tree.delete_item(values[scenario[del]], 0);
                //tree.dump();
                tree.walk(test_visit, nullptr);
                _RBTREE_TRACE("---\n");
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

            _RBTREE_TRACE("---\n");
        }
    }

    for (size_t pass = 0; pass < 2; ++pass) {
        rbtree_t tree;

        tree.init(&rbtree_t::test_cmp, nullptr);

        int chk_div = 12345;

        // Rotation stress
        for (unsigned i = 0; i < 65536 + 1024; ++i) {
            if (i < 65536)
                tree.insert(i, i + 42);
            if (i >= 1024)
                tree.delete_item(i - 1024, i - 1024 + 42);

            if (i != 65535 && --chk_div)
                continue;

            chk_div = 12345;

            unsigned k = 0;
            if (i >= 1024)
                k = i - 1024 + 1;
            for ( ; k <= i; ++k) {
                if (k >= 65536)
                    break;
                rbtree_t::iter_t it;
                rbtree_t::kvp_t needle{k, k + 42};
                rbtree_t::kvp_t *item = tree.find(&needle, &it);
                assert(item->key == k);
                assert(item->val == k + 42);
                rbtree_t::kvp_t& it_chk = tree.item(it);
                assert(it_chk.key == k);
                assert(it_chk.val == k + 42);
            }
        }
        assert(tree.size() == 0);
    }

    return 0;
}
