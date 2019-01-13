#pragma once
#include "types.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "utility.h"
#include "numeric_limits.h"
#include "memory.h"
#include "functional.h"
#include "printk.h"

__BEGIN_NAMESPACE_STD

#define _RBTREE_TRACE_ON    0
#if _RBTREE_TRACE_ON
#include "printk.h"
#define _RBTREE_TRACE(...) printk (__VA_ARGS__)
#else
#define _RBTREE_TRACE(...) ((void)0)
#endif

template<typename _T>
class rbtree_policy_t
{
public:
    //
    // Delete

    static constexpr int _RED = 1;
    static constexpr int _BLACK = 0;

    static _T *__sibling(_T *__n)
    {
        assert(__n);

        _T *__parent = __n->__parent;

        assert(__parent);

        if (__parent->__left == __n)
            return __parent->__right;

        return __parent->__left;
    }

    static _T *__uncle(_T *__n)
    {
        _T *__g = grandparent(__n);

        if (__g) {
            if (__n->__parent == __g->__left)
                return __g->__right;
            else
                return __g->__left;
        }

        return nullptr;
    }

    // Root can become black at any time, root must always be black
    static void __insert_case1(_T *__n)
    {
        _RBTREE_TRACE("Insert case 1\n");

        if (!__n->__parent)
            __n->__balance = _BLACK;
        else
            __insert_case2(__n);
    }

    // n is not the root,
    static void __insert_case2(_T *__n)
    {
        _RBTREE_TRACE("Insert case 2\n");

        if (__n->__parent->__balance != _BLACK)
            __insert_case3(__n);
    }

    static void __insert_case3(_T *__n)
    {
        _RBTREE_TRACE("Insert case 3\n");

        _T *__u = __uncle(__n);
        _T *__g;

        if (__u && __u->__balance == _RED) {
            __n->__parent->__balance = _BLACK;
            __u->__balance = _BLACK;
            __g = grandparent(__n);
            __g->__balance = _RED;
            __insert_case1(__g);
        } else {
            __insert_case4(__n);
        }
    }

    static void __insert_case4(_T *__n)
    {
        _RBTREE_TRACE("Insert case 4\n");

        _T *__g = grandparent(__n);

        if ((__n == __n->__parent->__right &&
             (__n->__parent == __g->__left))) {
            rotate_left(__n->__parent);
            __n = __n->__left;
        } else if ((__n == __n->__parent->__left) &&
                   (__n->__parent == __g->__right)) {
            rotate_right(__n->__parent);
            __n = __n->__right;
        }
        __insert_case5(__n);
    }

    static void __insert_case5(_T *__n)
    {
        _RBTREE_TRACE("Insert case 5\n");

        _T *__g = grandparent(__n);

        __n->__parent->__balance = _BLACK;
        __g->__balance = _RED;
        if (__n == __n->__parent->__left)
            rotate_right(__g);
        else
            rotate_left(__g);
    }

    static void __delete_case6(_T *__n)
    {
        _T *__nparent = __n->__parent;
        _T *__nsib = __sibling(__n);

        __nsib->__balance = __nparent->__balance;
        __nparent->__balance = _BLACK;
        if (__n == __nparent->__left) {
            assert(__nsib->__right->__balance == _RED);
            __nsib->__right->__balance = _BLACK;
            rotate_left(__nparent);
        } else {
            assert(__nsib->__left->__balance == _RED);
            __nsib->__left->__balance = _BLACK;
            rotate_right(__nparent);
        }
    }

    static void __delete_case5(_T *__n)
    {
        _T *__nparent = __n->__parent;
        _T *__nsib = __sibling(__n);

        if (__n == __nparent->__left &&
                __nsib->__balance == _BLACK &&
                __nsib->__left->__balance == _RED &&
                __nsib->__right->__balance == _BLACK) {
            __nsib->__balance = _RED;
            __nsib->__left->__balance = _BLACK;
            rotate_right(__nsib);
        } else if (__n == __nparent->__right &&
                   __nsib->__balance == _BLACK &&
                   __nsib->__right->__balance == _RED &&
                   __nsib->__left->__balance == _BLACK) {
            __nsib->__balance = _RED;
            __nsib->__right->__balance = _BLACK;
            rotate_left(__nsib);
        }
        __delete_case6(__n);
    }

    static void __delete_case4(_T *__n)
    {
        _T *__nparent = __n->__parent;
        _T *__nsib = __sibling(__n);

        if (__nparent->__balance == _RED &&
                __nsib->__balance == _BLACK &&
                __nsib->__left->__balance == _BLACK &&
                __nsib->__right->__balance == _BLACK) {
            __nsib->__balance = _RED;
            __nparent->__balance = _BLACK;
        } else {
            __delete_case5(__n);
        }
    }

    static void __delete_case3(_T *__n)
    {
        _T *__nparent = __n->__parent;
        _T *__nsib = __sibling(__n);

        if (__nparent->__balance == _BLACK &&
                __nsib->__balance == _BLACK &&
                __nsib->__left->__balance == _BLACK &&
                __nsib->__right->__balance == _BLACK) {
            __nsib->__balance = _RED;
            __delete_case1(__nparent);
        } else {
            __delete_case4(__n);
        }
    }

    static void __delete_case2(_T *__n)
    {
        _T *__nsib = __sibling(__n);

        if (__nsib->__balance == _RED) {
            _T *__nparent = __n->__parent;
            __nparent->__balance = _RED;
            __nsib->__balance = _BLACK;
            if (__n == __nparent->__left) {
                rotate_left(__nparent);
            } else {
                rotate_right(__nparent);
            }
        }
        __delete_case3(__n);
    }

    static void __delete_case1(_T *__n)
    {
        if (__n->__parent) {
            __delete_case2(__n);
        }
    }

    static void __swap_nodes(_T *&root, _T *__a, _T *__b)
    {
        assert(__a != __b);

        assert(__a->__parent || root == __a);
        assert(__b->__parent || root == __b);

        _T **__a_parent_ptr;
        _T **__b_parent_ptr;

        // Get the appropriate pointer to the parent's left/right pointer
        // or the tree root pointer

        __a_parent_ptr = __a->__parent
                ? (__a == __a->__parent->__left
                   ? &__a->__parent->__left
                   : &__a->__parent->__right)
                : &root;

        __b_parent_ptr = __b->__parent
                ? (__b == __b->__parent->__left
                   ? &__b->__parent->__left
                   : &__b->__parent->__right)
                : &root;

        assert(*__a_parent_ptr == __a);
        assert(*__b_parent_ptr == __b);

        std::swap(*__a_parent_ptr, *__b_parent_ptr);
        std::swap(__a->__parent, __b->__parent);
        std::swap(__a->__left, __b->__left);
        std::swap(__a->__right, __b->__right);
        std::swap(__a->__balance, __b->__balance);
    }

    static constexpr _T *tree_max(_T *n)
    {
        while (n->right)
            n = n->right;
        return n;
    }

    static constexpr _T *tree_min(_T *n)
    {
        while (n->left)
            n = n->left;
        return n;
    }

    static int __delete_at(_T *&__root, _T *__n)
    {
        _T *__child;
        _T *__left;
        _T *__right;

        __left = __n->__left;
        __right = __n->__right;

        if (__left && __right) {
            // Find highest value in left subtree
            _T *pred = tree_max(__left);

            // Swap tree links without physically moving any data ever
            __swap_nodes(__root, pred, __n);

            // Move the highest node in the left child
            // to this node and delete that node
            //__n->__kvp.key = pred->__kvp.key;
            //__n->__kvp.val = pred->__kvp.val;

            //__n = pred;
            __left = __n->__left;
            __right = __n->__right;
        }

        assert(!__left || !__right);
        __child = !__right ? __left : __right;
        if (__n->__balance == _BLACK) {
            __n->__balance = __child->__balance;
            __delete_case1(__n);
        }

        __replace_node(__root, __n, __child);

        if (!__n->__parent && __child)
            __child->__balance = _BLACK;

        free_node(__n);

        return 1;
    }

    static void __replace_node(_T *&__root, _T *__oldn, _T *__newn)
    {
        assert(__oldn != nullptr);

        _T *__oldp = __oldn->__parent;

        if (__oldp == nullptr) {
            __root = __newn;
        } else {
            if (__oldn == __oldp->__left)
                __oldp->__left = __newn;
            else
                __oldp->__right = __newn;
        }

        if (__newn)
            __newn->__parent = __oldp;
    }
};

template<typename _T>
class avltree_policy_t
{
public:
    static void retrace_insert(_T *&root_ptr, _T *_Z)
    {
        _T *_X, *_N, *_G;

        // Loop (possibly up to the root)
        for (_X = _Z->parent; _X != nullptr; _X = _Z->parent)
        {
            // X->balance has to be updated:
            if (_Z == _X->right)
            {
                // The right subtree increases
                if (_X->balance > 0)
                {
                    // X is right-heavy
                    // ===> the temporary X->balance == +2
                    // ===> rebalancing is required.
                    _G = _X->parent; // Save parent of X around rotations

                    if (_Z->balance < 0)
                    {
                        // Right Left Case     (see figure 5)
                        // Double rotation: Right(Z) then Left(X)
                        _N = __rotate_RightLeft(_X, _Z);
                    }
                    else
                    {
                        // Right Right Case    (see figure 4)
                        // Single rotation Left(X)
                        _N = __rotate_Left(_X, _Z);
                    }
                    // After rotation adapt parent link
                } else {
                    if (_X->balance < 0) {
                        // Z’s height increase is absorbed at X.
                        _X->balance = 0;
                        break; // Leave the loop
                    }

                    _X->balance = +1;
                    _Z = _X; // Height(Z) increases by 1
                    continue;
                }
            } else { // Z == left_child(X): the left subtree increases
                if (_X->balance < 0) { // X is left-heavy
                    // ===> the temporary X->balance == -2
                    // ===> rebalancing is required.
                    _G = _X->parent; // Save parent of X around rotations
                    if (_Z->balance > 0)      // Left Right Case
                        // Double rotation: Left(Z) then Right(X)
                        _N = __rotate_LeftRight(_X, _Z);
                    else                           // Left Left Case
                        // Single rotation Right(X)
                        _N = __rotate_Right(_X, _Z);
                    // After rotation adapt parent link
                } else {
                    if (_X->balance > 0) {
                        // Z’s height increase is absorbed at X.
                        _X->balance = 0;
                        break; // Leave the loop
                    }

                    _X->balance = -1;
                    _Z = _X; // Height(Z) increases by 1
                    continue;
                }
            }
            // After a rotation adapt parent link:
            // N is the new root of the rotated subtree
            // Height does not change: Height(N) == old Height(X)

            _N->parent = _G;

            if (_G != nullptr) {
                if (_X == _G->left)
                    _G->left = _N;
                else
                    _G->right = _N;
                break;
            } else {
                root_ptr = _N; // N is the new root of the total tree
                break;
            }
            // There is no fall thru, only break; or continue;
        }

        // Unless loop is left via break,
        // the height of the total tree increases by 1.
    }

    static void retrace_delete(_T *&__root_ptr, _T *_N)
    {
        _T *_X, *_G, *_Z;
        int __b;

        for (_X = _N->parent; _X != nullptr; _X = _G) {
            // Loop (possibly up to the root)
            // Save parent of X around rotations
            _G = _X->parent;
            // X->balance has not yet been updated!
            if (_N == _X->left) {
                // the left subtree decreases
                if (_X->balance > 0) {
                    // X is right-heavy
                    // ===> the temporary X->balance == +2
                    // ===> rebalancing is required.
                    _Z = _X->right; // Sibling of N (higher by 2)
                    __b = _Z->balance;
                    if (__b < 0)
                        // Right Left Case     (see figure 5)
                        // Double rotation: Right(Z) then Left(X)
                        _N = __rotate_RightLeft(_X, _Z);
                    else
                        // Right Right Case    (see figure 4)
                        // Single rotation Left(X)
                        _N = __rotate_Left(_X, _Z);

                    // After rotation adapt parent link
                } else {
                    if (_X->balance == 0) {
                        // N’s height decrease is absorbed at X.
                        _X->balance = +1;
                        // Leave the loop
                        break;
                    }
                    _N = _X;
                    _N->balance = 0; // Height(N) decreases by 1
                    continue;
                }
            } else {
                // (N == X->right): The right subtree decreases
                if (_X->balance < 0) {
                    // X is left-heavy
                    // ===> the temporary X->balance == -2
                    // ===> rebalancing is required.
                    // Sibling of N (higher by 2)
                    _Z = _X->left;
                    __b = _Z->balance;
                    if (__b > 0)
                        // Left Right Case
                        // Double rotation: Left(Z) then Right(X)
                        _N = __rotate_LeftRight(_X, _Z);
                    else
                        // Left Left Case
                        // Single rotation Right(X)
                        _N = __rotate_Right(_X, _Z);
                    // After rotation adapt parent link
                } else {
                    if (_X->balance == 0) {
                        // N’s height decrease is absorbed at X.
                        _X->balance = -1;
                        // Leave the loop
                        break;
                    }
                    _N = _X;
                    // Height(N) decreases by 1
                    _N->balance = 0;
                    continue;
                }
            }
            // After a rotation adapt parent link:
            // N is the new root of the rotated subtree
            _N->parent = _G;
            if (_G != nullptr) {
                if (_X == _G->left)
                    _G->left = _N;
                else
                    _G->right = _N;
                if (__b == 0)
                    // Height does not change: Leave the loop
                    break;
            } else {
                // N is the new root of the total tree
                __root_ptr = _N;
            }
            // Height(N) decreases by 1 (== old Height(X)-1)
        }
        // Unless loop is left via break,
        // the height of the total tree decreases by 1.
    }

    static _T *__rotate_RightLeft(_T *_X, _T *_Z)
    {
        _T *_Y, *__t3, *__t2;

        // Z is by 2 higher than its sibling
        _Y = _Z->left; // Inner child of Z
        // Y is by 1 higher than sibling
        __t3 = _Y->right;
        _Z->left = __t3;

        if (__t3 != nullptr)
            __t3->parent = _Z;

        _Y->right = _Z;
        _Z->parent = _Y;
        __t2 = _Y->left;
        _X->right = __t2;

        if (__t2 != nullptr)
            __t2->parent = _X;

        _Y->left = _X;
        _X->parent = _Y;

        if (_Y->balance > 0) {
            // t3 was higher
            // t1 now higher
            _X->balance = -1;
            _Z->balance = 0;
        } else if (_Y->balance == 0) {
            _X->balance = 0;
            _Z->balance = 0;
        } else {
            // t2 was higher
            _X->balance = 0;
            // t4 now higher
            _Z->balance = +1;
        }

        _Y->balance = 0;
        // return new root of rotated subtree
        return _Y;
    }

    static _T *__rotate_LeftRight(_T *_X, _T *_Z)
    {
        _T *_Y, *__t3, *__t2;

        // Z is by 2 higher than its sibling
        _Y = _Z->right; // Inner child of Z
        // Y is by 1 higher than sibling
        __t3 = _Y->left;
        _Z->right = __t3;

        if (__t3 != nullptr)
            __t3->parent = _Z;

        _Y->left = _Z;
        _Z->parent = _Y;
        __t2 = _Y->right;
        _X->left = __t2;

        if (__t2 != nullptr)
            __t2->parent = _X;

        _Y->right = _X;
        _X->parent = _Y;

        if (_Y->balance > 0) {
            // t3 was higher
            // t1 now higher
            _X->balance = -1;
            _Z->balance = 0;
        } else if (_Y->balance == 0) {
            _X->balance = 0;
            _Z->balance = 0;
        } else {
            // t2 was higher
            _X->balance = 0;
            _Z->balance = +1;  // t4 now higher
        }

        _Y->balance = 0;

        // return new root of rotated subtree
        return _Y;
    }

    static _T *__rotate_Right(_T *_X, _T *_Z)
    {
        // Z is by 2 higher than its sibling

        // Inner child of Z
        _T *__t23 = _Z->right;
        _X->left = __t23;

        if (__t23 != nullptr)
            __t23->parent = _X;

        _Z->right = _X;
        _X->parent = _Z;

        // 1st case, Z->balance == 0,
        // only happens with deletion, not insertion:
        if (_Z->balance == 0) {
            // t23 has been of same height as t4
            _X->balance = +1;   // t23 now higher
            _Z->balance = -1;   // t4 now lower than X
        } else {
            // 2nd case happens with insertion or deletion:
            _X->balance = 0;
            _Z->balance = 0;
        }

        // return new root of rotated subtree
        return _Z;
    }

    static _T *__rotate_Left(_T *_X, _T *_Z)
    {
        // Z is by 2 higher than its sibling

        // Inner child of Z
        _T *__t23 = _Z->left;
        _X->right = __t23;

        if (__t23 != nullptr)
            __t23->parent = _X;

        _Z->left = _X;
        _X->parent = _Z;

        // 1st case, Z->balance == 0,
        // only happens with deletion, not insertion:
        if (_Z->balance == 0) {
            // t23 has been of same height as t4
            _X->balance = +1;   // t23 now higher
            _Z->balance = -1;   // t4 now lower than X
        } else {
            // 2nd case happens with insertion or deletion:
            _X->balance = 0;
            _Z->balance = 0;
        }

        // return new root of rotated subtree
        return _Z;
    }
};

template<>
class avltree_policy_t<void>
{
public:
    template<typename U>
    using rebind = avltree_policy_t<U>;
};

template<typename T>
class panic_policy_t
{
public:
    static T *oom()
    {
        panic_oom();
        __builtin_unreachable();
    }
};

template<
    typename _T,
    typename _Compare = less<_T>, typename _Alloc = allocator<_T>,
    typename _TreePolicy = avltree_policy_t<void>,
    typename _OOMPolicy = panic_policy_t<_T>>
class basic_set
{
private:
    struct node_t;
public:
    using value_type = _T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = _T*;
    using const_pointer = _T const*;
    using reference = _T&;
    using const_reference = _T const&;

    template<bool _is_const, int dir>
    class basic_iterator
    {
    public:
        basic_iterator() = default;
        basic_iterator(basic_iterator const& rhs) = default;
        basic_iterator &operator=(basic_iterator const& rhs) = default;

        template<bool _rhs_is_const, int _dir>
        basic_iterator(basic_iterator<_rhs_is_const, _dir> const& rhs)
            : curr(rhs.curr)
        {
            // --------+---------+---------
            // LHS     | RHS     | Allowed
            // --------+---------+---------
            // mutable | mutable | yes
            // mutable | const   | no
            // const   | mutable | yes
            // const   | const   | yes
            // --------+---------+---------

            static_assert(_is_const || !_rhs_is_const,
                "Cannot copy const_(reverse_)iterator to (reverse_)iterator");
        }

        // ++ at end has no effect
        basic_iterator& operator++()
        {
            if (curr) {
                if (curr->right) {
                    // Find lowest value in right subtree
                    curr = curr->right;

                    while (curr->left)
                        curr = curr->left;
                } else {
                    // Go up at least to parent, keep going while from right
                    node_t *p = curr->parent;
                    while (p && curr == p->right) {
                        curr = p;
                        p = p->parent;
                    }

                    curr = p;
                }
            }
            return *this;
        }

        basic_iterator& operator--()
        {
            if (curr) {
                if (curr->left) {
                    // Find highest value in left subtree
                    curr = curr->left;

                    while (curr->right)
                        curr = curr->right;

                    return *this;
                }

                // Go up at least to parent, keep going while from left
                node_t *p = curr->parent;
                while (p && curr == p->left) {
                    curr = p;
                    p = p->parent;
                }

                curr = p;
            }

            return *this;
        }

        basic_iterator operator++(int)
        {
            basic_iterator orig(*this);
            ++*this;
            return orig;
        }

        basic_iterator operator--(int)
        {
            basic_iterator orig(*this);
            --*this;
            return orig;
        }

        reference operator*()
        {
            return curr->item();
        }

        const_reference operator*() const
        {
            return curr->item();
        }

        pointer operator->()
        {
            return curr->item_ptr();
        }

        const_pointer operator->() const
        {
            return curr->item_ptr();
        }

        bool operator==(basic_iterator const& rhs)
        {
            return curr == rhs.curr;
        }

        bool operator!=(basic_iterator const& rhs)
        {
            return curr != rhs.curr;
        }

    private:
        friend class basic_set;

        basic_iterator(node_t *node)
            : curr(node)
        {
        }

        node_t *curr;
    };

    using iterator = basic_iterator<false, false>;
    using const_iterator = basic_iterator<true, false>;
    using reverse_iterator = basic_iterator<false, true>;
    using const_reverse_iterator = basic_iterator<true, true>;

    basic_set()
        : root(nullptr)
        , current_size(0)
    {

    }

    basic_set(basic_set const& rhs)
        : root(nullptr)
        , current_size(0)
    {
        for (value_type& item : rhs)
            insert(item);
    }

    basic_set(basic_set&& rhs)
        : root(rhs.root)
        , current_size(rhs.current_size)
    {
        rhs.root = nullptr;
        rhs.current_size = 0;
    }

    basic_set& operator=(basic_set const& rhs)
    {
        if (rhs != this) {
            clear();
            for (value_type& item : rhs)
                insert(item);
        }
    }

    size_type size() const
    {
        return size;
    }

    bool empty() const
    {
        return current_size == 0;
    }

    iterator begin()
    {
        node_t *n = tree_min();
        return iterator(n);
    }

    const_iterator begin() const
    {
        node_t *n = tree_min();
        return const_iterator(n);
    }

    const_iterator cbegin() const
    {
        node_t *n = tree_min();
        return const_iterator(n);
    }

    reverse_iterator rbegin()
    {
        node_t *n = tree_max();
        return reverse_iterator(n);
    }

    const_reverse_iterator rbegin() const
    {
        node_t *n = tree_max();
        return const_reverse_iterator(n);
    }

    const_reverse_iterator crbegin() const
    {
        node_t *n = tree_max();
        return const_reverse_iterator(n);
    }

    iterator end()
    {
        return iterator(nullptr);
    }

    const_iterator end() const
    {
        return const_iterator(nullptr);
    }

    const_iterator cend() const
    {
        return const_iterator(nullptr);
    }

    reverse_iterator rend()
    {
        return reverse_iterator(nullptr);
    }

    const_reverse_iterator rend() const
    {
        return const_reverse_iterator(nullptr);
    }

    const_reverse_iterator crend() const
    {
        return const_reverse_iterator(nullptr);
    }

    size_type max_size()
    {
        return std::numeric_limits<size_t>::max() /
                (sizeof(node_t) + 32);
    }

    void swap(basic_set& rhs)
    {
        std::swap(root, rhs.root);
        std::swap(current_size, rhs.current_size);
    }

    void clear()
    {
        iterator next;
        for (iterator it = begin(), en = end(); it != en; it = next)
        {
            next = it;
            ++next;

            it.curr->item_ptr()->~value_type();
            alloc.deallocate(it.curr, sizeof(*it.curr));
        }
    }

    iterator find(_T const& k)
    {
        return iterator(const_cast<node_t*>(__tree_find(k)));
    }

    const_iterator find(_T const& k) const
    {
        return const_iterator(__tree_find(k));
    }

    template<typename U>
    iterator find(U const& k)
    {
        static_assert(sizeof(typename _Compare::is_transparent) != -1,
                      "C++14 find requires comparator type to have"
                      " is_transparent member type");

        return iterator(const_cast<node_t*>(__tree_find(k)));
    }

    template<typename U>
    const_iterator find(U const& k) const
    {
        static_assert(sizeof(typename _Compare::is_transparent) != -1,
                      "C++14 find requires comparator type to have"
                      " is_transparent member type");

        return const_iterator(__tree_find(k));
    }

    pair<iterator, bool> insert(_T&& value)
    {
        bool found_dup;
        node_t *i = __tree_ins(nullptr, _Compare(), found_dup, &value, true);

        if (i != nullptr)
        {
            TreePolicy::retrace_insert(root, i);
        }
    }

    pair<iterator, bool> insert(_T const& value)
    {
        return emplace(value);
    }

    template<typename... Args>
    pair<iterator, bool> emplace(Args&& ...args)
    {
        // Allocate memory for node using allocator
        void* n_mem = alloc.allocate(sizeof(node_t));

        // Placement new into memory
        node_t *n = new (n_mem) node_t;

        // Forwarding placement construct item in-place
        value_type *ins = new (reinterpret_cast<pointer>(n->storage.data))
                value_type(std::forward<Args>(args)...);

        bool found_dup;
        node_t *i = __tree_ins(n, _Compare(), found_dup, ins, false);

        if (likely(!found_dup))
            TreePolicy::retrace_insert(root, n);

        return { iterator(i), !found_dup };
    }

    iterator erase(const_iterator place)
    {
        node_t *node = place.curr;

        iterator result(node);
        if (!(++result).curr)
            --result;

        TreePolicy::retraceDelete(root, node);

        assert(node->left == nullptr);
        assert(node->right == nullptr);
        if (node->parent)
            node->parent->select_lr(node->is_left_child()) = nullptr;
        else
            root = nullptr;

        return result;
    }

    template<typename _K, typename _C>
    iterator lower_bound(_K const& k, _C comparator)
    {
        node_t *node = root;

        while (node) {
            auto& item = node->item();

            bool is_lt = _Compare()(k, item);
            bool is_gt = _Compare()(item, k);

            if (unlikely((is_lt | is_gt) == 0))
                break;

            node_t *next_node = node->select_lr(is_lt);

            if (unlikely(!next_node))
                break;

            node = next_node;
        }

        return iterator(node);
    }

    template<typename _K, typename _C>
    iterator upper_bound(_K const& key, _C comparator)
    {

    }

private:

    struct node_t {
        node_t *left;
        node_t *right;
        node_t *parent;
        int balance;
        typename std::aligned_storage<sizeof(_T), alignof(_T)>::type storage;

        inline reference item() noexcept
        {
            return *reinterpret_cast<pointer>(storage.data);
        }

        inline const_reference item() const noexcept
        {
            return *reinterpret_cast<const_pointer>(storage.data);
        }

        inline pointer item_ptr() noexcept
        {
            return reinterpret_cast<pointer>(storage.data);
        }

        inline const_pointer item_ptr() const noexcept
        {
            return reinterpret_cast<const_pointer>(storage.data);
        }

        inline node_t * const& select_lr(bool is_left) const
        {
            return is_left ? left : right;
        }

        inline node_t *&select_lr(bool is_left) noexcept
        {
            return is_left ? left : right;
        }

        inline bool is_left_child() const noexcept
        {
            return parent->left == this;
        }

        inline bool is_right_child() const noexcept
        {
            return parent->right == this;
        }
    };

    // Find an existing key, null if no exact match
    template<typename K>
    node_t const *__tree_find(K const& k) const
    {
        node_t const *node = root;

        while (node) {
            auto& item = node->item();

            bool is_lt = _Compare()(k, item);
            bool is_gt = _Compare()(item, k);

            if (unlikely((is_lt | is_gt) == 0))
                break;

            node = node->select_lr(is_lt);
        }

        return node;
    }

    // Find the insertion point and insert the node
    // Sets found_dup to true if an equal node was found
    // Returns the found existing node or newly inserted node
    template<typename _Cmp>
    node_t *__tree_ins(node_t *n, _Cmp const& cmp, bool &found_dup,
                       _T *val, bool use_move)
    {
        found_dup = false;

        node_t *s;
        bool is_left;

        // Try hard to avoid doing any memory
        // allocation or copy for failed insert

        if (root)
        {
            node_t *next = root;

            do {
                s = next;
                auto&& lhs = *val;
                auto&& rhs = s->item();

                is_left = cmp(lhs, rhs);

                // Check for equality
                bool is_right = cmp(rhs, lhs);

                if (unlikely((is_left | is_right) == 0))
                {
                    found_dup = true;
                    return s;
                }

                next = is_left ? s->left : s->right;
            } while (next);
        } else {
            // No parent, new node is root
            s = nullptr;
        }

        if (n == nullptr)
        {
            // Allocate memory for node using allocator
            void* n_mem = alloc.allocate(sizeof(node_t));

            if (!n_mem)
                _OOMPolicy::oom();

            // Placement new into memory
            n = new (n_mem) node_t;

            // Forwarding placement construct item
            if (use_move)
                new (reinterpret_cast<pointer>(n->storage.data))
                        value_type(move(*val));
            else
                new (reinterpret_cast<pointer>(n->storage.data))
                        value_type(*val);
        }

        // New node is initially a leaf
        n->left = nullptr;
        n->right = nullptr;
        n->balance = 0;

        n->parent = s;
        ++current_size;

        if (s)
            s->select_lr(is_left) = n;
        else
            root = n;

        return n;
    }

    using TreePolicy = typename avltree_policy_t<void>::template rebind<node_t>;

    node_t *tree_min() const
    {
        node_t *n = root;

        if (n) {
            while (n->left)
                n = n->left;
        }
        return n;
    }

    node_t *tree_max() const
    {
        node_t *n = root;

        if (n) {
            while (n->left)
                n = n->left;
        }
        return n;
    }

public:
    static void test()
    {
        basic_set chk;

        for (_T i = 0; i < 100; ++i)
        {
            chk.insert(i);

            auto it = chk.find(i);

            chk.erase(it);
        }

        for (_T& i : chk)
        {
            printdbg("%d\n", i);
        }
    }

private:
    node_t *root;
    size_t current_size;
    _Alloc alloc;
};

template<typename T, typename C = less<T>, typename A = allocator<T>>
using set = basic_set<T, C, A>;

__END_NAMESPACE_STD

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

    constexpr rbtree_t();
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
    kvp_t *find(kvp_t *__kvp, iter_t *__iter) const;

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

#define _NODE(i) (__nodes + (i))

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
constexpr rbtree_t<_Tkey,_Tval>::rbtree_t()
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

template<typename _Tkey, typename _Tval>
typename rbtree_t<_Tkey,_Tval>::kvp_t *
rbtree_t<_Tkey,_Tval>::find(kvp_t *__kvp, iter_t *__iter) const
{
    return const_cast<rbtree_t<_Tkey,_Tval>*>(this)->find(__kvp, __iter);
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
    //_RBTREE_TRACE("Item: key=%d val=%d\n", __kvp->key, __kvp->val);
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

