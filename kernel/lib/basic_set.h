#pragma once
#include "types.h"
#include "utility.h"
#include "type_traits.h"
#include "functional.h"
#include "numeric_limits.h"
#include "debug.h"
#include "printk.h"

#define _BASIC_SET_TRACE_ON    0
#if _BASIC_SET_TRACE_ON
#include "printk.h"
#define _BASIC_SET_TRACE(...) printk (__VA_ARGS__)
#else
#define _BASIC_SET_TRACE(...) ((void)0)
#endif

__BEGIN_NAMESPACE_STD

namespace detail {

template<typename _T>
class rbtree_policy_t
{
public:
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
        _BASIC_SET_TRACE("Insert case 1\n");

        if (!__n->__parent)
            __n->__balance = _BLACK;
        else
            __insert_case2(__n);
    }

    // n is not the root,
    static void __insert_case2(_T *__n)
    {
        _BASIC_SET_TRACE("Insert case 2\n");

        if (__n->__parent->__balance != _BLACK)
            __insert_case3(__n);
    }

    static void __insert_case3(_T *__n)
    {
        _BASIC_SET_TRACE("Insert case 3\n");

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
        _BASIC_SET_TRACE("Insert case 4\n");

        _T *__g = grandparent(__n);

        if ((__n == __n->__parent->__right &&
             (__n->__parent == __g->__left))) {
            __rotate_left(__n->__parent);
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
        _BASIC_SET_TRACE("Insert case 5\n");

        _T *__g = grandparent(__n);

        __n->__parent->__balance = _BLACK;
        __g->__balance = _RED;
        if (__n == __n->__parent->__left)
            rotate_right(__g);
        else
            __rotate_left(__g);
    }

    //
    // Delete

    static void __delete_case6(_T *__n)
    {
        _T *__nparent = __n->__parent;
        _T *__nsib = __sibling(__n);

        __nsib->__balance = __nparent->__balance;
        __nparent->__balance = _BLACK;
        if (__n == __nparent->__left) {
            assert(__nsib->__right->__balance == _RED);
            __nsib->__right->__balance = _BLACK;
            __rotate_left(__nparent);
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
            __rotate_left(__nsib);
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
                __rotate_left(__nparent);
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

    static constexpr _T *__tree_max(_T *n)
    {
        while (n->__right)
            n = n->__right;
        return n;
    }

    static constexpr _T *tree_min(_T *n)
    {
        while (n->__left)
            n = n->__left;
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
            _T *pred = __tree_max(__left);

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
        for (_X = _Z->__parent; _X != nullptr; _X = _Z->__parent) {
            // X->balance has to be updated:
            if (_Z == _X->__right) {
                // The right subtree increases
                if (_X->__balance > 0) {
                    // X is right-heavy
                    // ===> the temporary X->balance == +2
                    // ===> rebalancing is required.
                    _G = _X->__parent; // Save parent of X around rotations

                    if (_Z->__balance < 0) {
                        // Right Left Case     (see figure 5)
                        // Double rotation: Right(Z) then Left(X)
                        _N = __rotate_rightleft(_X, _Z);
                    } else {
                        // Right Right Case    (see figure 4)
                        // Single rotation Left(X)
                        _N = __rotate_left(_X, _Z);
                    }
                    // After rotation adapt parent link
                } else {
                    if (_X->__balance < 0) {
                        // Z’s height increase is absorbed at X.
                        _X->__balance = 0;
                        break; // Leave the loop
                    }

                    _X->__balance = +1;
                    _Z = _X; // Height(Z) increases by 1
                    continue;
                }
            } else { // Z == left_child(X): the left subtree increases
                if (_X->__balance < 0) { // X is left-heavy
                    // ===> the temporary X->balance == -2
                    // ===> rebalancing is required.
                    // Save parent of X around rotations
                    _G = _X->__parent;
                    // Left Right Case
                    if (_Z->__balance > 0)
                        // Double rotation: Left(Z) then Right(X)
                        _N = __rotate_leftright(_X, _Z);
                    else                           // Left Left Case
                        // Single rotation Right(X)
                        _N = __rotate_right(_X, _Z);
                    // After rotation adapt parent link
                } else {
                    if (_X->__balance > 0) {
                        // Z’s height increase is absorbed at X.
                        _X->__balance = 0;
                        break; // Leave the loop
                    }

                    _X->__balance = -1;
                    _Z = _X; // Height(Z) increases by 1
                    continue;
                }
            }
            // After a rotation adapt parent link:
            // N is the new root of the rotated subtree
            // Height does not change: Height(N) == old Height(X)

            _N->__parent = _G;

            if (_G != nullptr) {
                if (_X == _G->__left)
                    _G->__left = _N;
                else
                    _G->__right = _N;
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

    static void __retrace_delete(_T *&__root_ptr, _T *_N)
    {
        _T *_X, *_G, *_Z;
        int __b;

        for (_X = _N->__parent; _X != nullptr; _X = _G) {
            // Loop (possibly up to the root)
            // Save parent of X around rotations
            _G = _X->__parent;
            // X->__balance has not yet been updated!
            if (_N == _X->__left) {
                // the left subtree decreases
                if (_X->__balance > 0) {
                    // X is right-heavy
                    // ===> the temporary X->__balance == +2
                    // ===> rebalancing is required.
                    _Z = _X->__right; // Sibling of N (higher by 2)
                    __b = _Z->__balance;
                    if (__b < 0)
                        // Right Left Case     (see figure 5)
                        // Double rotation: Right(Z) then Left(X)
                        _N = __rotate_rightleft(_X, _Z);
                    else
                        // Right Right Case    (see figure 4)
                        // Single rotation Left(X)
                        _N = __rotate_left(_X, _Z);

                    // After rotation adapt parent link
                } else {
                    if (_X->__balance == 0) {
                        // N’s height decrease is absorbed at X.
                        _X->__balance = +1;
                        // Leave the loop
                        break;
                    }
                    _N = _X;
                    _N->__balance = 0; // Height(N) decreases by 1
                    continue;
                }
            } else {
                // (N == X->__right): The right subtree decreases
                if (_X->__balance < 0) {
                    // X is left-heavy
                    // ===> the temporary X->__balance == -2
                    // ===> rebalancing is required.
                    // Sibling of N (higher by 2)
                    _Z = _X->__left;
                    __b = _Z->__balance;
                    if (__b > 0)
                        // Left Right Case
                        // Double rotation: Left(Z) then Right(X)
                        _N = __rotate_leftright(_X, _Z);
                    else
                        // Left Left Case
                        // Single rotation Right(X)
                        _N = __rotate_right(_X, _Z);
                    // After rotation adapt parent link
                } else {
                    if (_X->__balance == 0) {
                        // N’s height decrease is absorbed at X.
                        _X->__balance = -1;
                        // Leave the loop
                        break;
                    }
                    _N = _X;
                    // Height(N) decreases by 1
                    _N->__balance = 0;
                    continue;
                }
            }
            // After a rotation adapt parent link:
            // N is the new root of the rotated subtree
            _N->__parent = _G;
            if (_G != nullptr) {
                if (_X == _G->__left)
                    _G->__left = _N;
                else
                    _G->__right = _N;
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

    static _T *__rotate_rightleft(_T *_X, _T *_Z)
    {
        _T *_Y, *__t3, *__t2;

        // Z is by 2 higher than its sibling
        _Y = _Z->__left; // Inner child of Z
        // Y is by 1 higher than sibling
        __t3 = _Y->__right;
        _Z->__left = __t3;

        if (__t3 != nullptr)
            __t3->__parent = _Z;

        _Y->__right = _Z;
        _Z->__parent = _Y;
        __t2 = _Y->__left;
        _X->__right = __t2;

        if (__t2 != nullptr)
            __t2->__parent = _X;

        _Y->__left = _X;
        _X->__parent = _Y;

        if (_Y->__balance > 0) {
            // t3 was higher
            // t1 now higher
            _X->__balance = -1;
            _Z->__balance = 0;
        } else if (_Y->__balance == 0) {
            _X->__balance = 0;
            _Z->__balance = 0;
        } else {
            // t2 was higher
            _X->__balance = 0;
            // t4 now higher
            _Z->__balance = +1;
        }

        _Y->__balance = 0;
        // return new root of rotated subtree
        return _Y;
    }

    static _T *__rotate_leftright(_T *_X, _T *_Z)
    {
        _T *_Y, *__t3, *__t2;

        // Z is by 2 higher than its sibling
        _Y = _Z->__right; // Inner child of Z
        // Y is by 1 higher than sibling
        __t3 = _Y->__left;
        _Z->__right = __t3;

        if (__t3 != nullptr)
            __t3->__parent = _Z;

        _Y->__left = _Z;
        _Z->__parent = _Y;
        __t2 = _Y->__right;
        _X->__left = __t2;

        if (__t2 != nullptr)
            __t2->__parent = _X;

        _Y->__right = _X;
        _X->__parent = _Y;

        if (_Y->__balance > 0) {
            // t3 was higher
            // t1 now higher
            _X->__balance = -1;
            _Z->__balance = 0;
        } else if (_Y->__balance == 0) {
            _X->__balance = 0;
            _Z->__balance = 0;
        } else {
            // t2 was higher
            _X->__balance = 0;
            _Z->__balance = +1;  // t4 now higher
        }

        _Y->__balance = 0;

        // return new root of rotated subtree
        return _Y;
    }

    static _T *__rotate_right(_T *_X, _T *_Z)
    {
        // Z is by 2 higher than its sibling

        // Inner child of Z
        _T *__t23 = _Z->__right;
        _X->__left = __t23;

        if (__t23 != nullptr)
            __t23->__parent = _X;

        _Z->__right = _X;
        _X->__parent = _Z;

        // 1st case, Z->__balance == 0,
        // only happens with deletion, not insertion:
        if (_Z->__balance == 0) {
            // t23 has been of same height as t4
            _X->__balance = +1;   // t23 now higher
            _Z->__balance = -1;   // t4 now lower than X
        } else {
            // 2nd case happens with insertion or deletion:
            _X->__balance = 0;
            _Z->__balance = 0;
        }

        // return new root of rotated subtree
        return _Z;
    }

    static _T *__rotate_left(_T *_X, _T *_Z)
    {
        // Z is by 2 higher than its sibling

        // Inner child of Z
        _T *__t23 = _Z->__left;
        _X->__right = __t23;

        if (__t23 != nullptr)
            __t23->__parent = _X;

        _Z->__left = _X;
        _X->__parent = _Z;

        // 1st case, Z->__balance == 0,
        // only happens with deletion, not insertion:
        if (_Z->__balance == 0) {
            // t23 has been of same height as t4
            _X->__balance = +1;   // t23 now higher
            _Z->__balance = -1;   // t4 now lower than X
        } else {
            // 2nd case happens with insertion or deletion:
            _X->__balance = 0;
            _Z->__balance = 0;
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

}   // namespace detail

template<
    typename _T,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<_T>,
    typename _TreePolicy = detail::avltree_policy_t<void>,
    typename _OOMPolicy = detail::panic_policy_t<_T>>
class basic_set
{
private:
    struct node_t;
    using node_allocator_t = typename _Alloc::template rebind<node_t>::other;
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
    private:
        static_assert(dir == 1 || -dir == 1, "Unexpected direction");
        using owner_ptr_t = basic_set const *;
        using node_ptr_t = node_t const *;

        template<bool _is_const_inst>
        typename enable_if<!_is_const_inst, node_t *>::type ptr()
        {
            return const_cast<node_t*>(curr);
        }

    public:
        basic_iterator() = default;
        basic_iterator(basic_iterator const& rhs) = default;
        basic_iterator &operator=(basic_iterator const& rhs) = default;

        template<bool _rhs_is_const, int _dir>
        basic_iterator(basic_iterator<_rhs_is_const, _dir> const& rhs)
            : curr(rhs.curr)
            , owner(rhs.owner)
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

        // Step to the element that compares greater
        basic_iterator& step(integral_constant<int, 1>::type const&)
        {
            if (curr) {
                if (curr->__right) {
                    // Find lowest value in right subtree
                    curr = curr->__right;

                    while (curr->__left)
                        curr = curr->__left;
                } else {
                    // Go up at least to parent, keep going while from right
                    node_t *p = curr->__parent;
                    while (p && curr == p->__right) {
                        curr = p;
                        p = p->__parent;
                    }

                    curr = p;
                }
            }

            return *this;
        }

        // Step to the element that compares lesser
        basic_iterator& step(integral_constant<int, -1>::type const&)
        {
            if (curr) {
                if (curr->__left) {
                    // Find highest value in left subtree
                    curr = curr->__left;

                    while (curr->__right)
                        curr = curr->__right;

                    return *this;
                }

                // Go up at least to parent, keep going while from left
                node_t *p = curr->__parent;
                while (p && curr == p->__left) {
                    curr = p;
                    p = p->__parent;
                }

                curr = p;
            }

            return *this;
        }

        _always_inline basic_iterator& inc()
        {
            return step(typename integral_constant<int, dir>::type());
        }

        _always_inline basic_iterator& dec()
        {
            // Steps to lesser if forward, greater if reverse
            return step(typename integral_constant<int, -dir>::type());
        }

        _always_inline basic_iterator& operator++()
        {
            return inc();
        }

        _always_inline basic_iterator& operator--()
        {
            return dec();
        }

        basic_iterator operator++(int)
        {
            basic_iterator orig(*this);
            inc();
            return orig;
        }

        basic_iterator operator--(int)
        {
            basic_iterator orig(*this);
            dec();
            return orig;
        }

        const_reference operator*() const
        {
            return curr->item();
        }

        const_pointer operator->() const
        {
            return curr->item_ptr();
        }

        bool operator==(basic_iterator const& rhs) const
        {
            return curr == rhs.curr;
        }

        bool operator!=(basic_iterator const& rhs) const
        {
            return curr != rhs.curr;
        }

        basic_iterator operator-(size_t n) const
        {
            basic_iterator result(*this);

            if (curr == nullptr && n) {
                --n;
                if (dir > 0)
                    result.curr = owner->tree_max();
                else
                    result.curr = owner->tree_min();
            }

            while (n-- && result.curr)
                --result;

            return result;
        }

        basic_iterator operator+(size_t n) const
        {
            basic_iterator result(*this);
            while (n-- && result.curr)
                ++result;
            return result;
        }

    private:
        friend class basic_set;

        basic_iterator(node_ptr_t node,
                       owner_ptr_t owner)
            : curr(node)
            , owner(owner)
        {
        }

        node_ptr_t curr = nullptr;
        owner_ptr_t owner = nullptr;
    };

    // Set interestingly only has const iterators
    using const_iterator = basic_iterator<true, 1>;
    using reverse_iterator = basic_iterator<true, -1>;
    using iterator = const_iterator;
    using const_reverse_iterator = reverse_iterator;

    basic_set()
        : __root(nullptr)
        , __current_size(0)
    {

    }

    ~basic_set()
    {
        clear();
    }

    basic_set(basic_set const& rhs)
        : __root(nullptr)
        , __current_size(0)
    {
        for (value_type const& item : rhs)
            insert(item);
    }

    basic_set(basic_set&& rhs)
        : __root(rhs.__root)
        , __current_size(rhs.__current_size)
    {
        rhs.__root = nullptr;
        rhs.__current_size = 0;
    }

    basic_set& operator=(basic_set const& rhs)
    {
        if (&rhs != this) {
            clear();
            for (value_type const& item : rhs)
                insert(item);
        }
        return *this;
    }

    size_type size() const
    {
        return __current_size;
    }

    bool empty() const
    {
        return __current_size == 0;
    }

    ///     (nullptr)     (max)
    ///       rend       rbegin
    ///        |           |
    ///       ~~~+---+---+---+~~~
    ///      :   | V | V | V |   :
    ///       ~~~+---+---+---+~~~
    ///            |           |
    ///          begin        end
    ///           (min)    (nullptr)

    iterator begin()
    {
        node_t *n = tree_min();
        return iterator(n, this);
    }

    const_iterator begin() const
    {
        node_t *n = tree_min();
        return const_iterator(n, this);
    }

    const_iterator cbegin() const
    {
        node_t *n = tree_min();
        return const_iterator(n, this);
    }

    reverse_iterator rbegin()
    {
        node_t *n = tree_max();
        return reverse_iterator(n, this);
    }

    const_reverse_iterator rbegin() const
    {
        node_t *n = tree_max();
        return const_reverse_iterator(n, this);
    }

    const_reverse_iterator crbegin() const
    {
        node_t *n = tree_max();
        return const_reverse_iterator(n, this);
    }

    iterator end()
    {
        return iterator(nullptr, this);
    }

    const_iterator end() const
    {
        return const_iterator(nullptr, this);
    }

    const_iterator cend() const
    {
        return const_iterator(nullptr, this);
    }

    reverse_iterator rend()
    {
        return reverse_iterator(nullptr, this);
    }

    const_reverse_iterator rend() const
    {
        return const_reverse_iterator(nullptr, this);
    }

    const_reverse_iterator crend() const
    {
        return const_reverse_iterator(nullptr, this);
    }

    size_type max_size()
    {
        return std::numeric_limits<size_t>::max() /
                (sizeof(node_t) + 32);
    }

    void swap(basic_set& rhs)
    {
        std::swap(__root, rhs.__root);
        std::swap(__current_size, rhs.__current_size);
    }

    void clear()
    {
        for (node_t *next, *node = __root; node; node = next) {
            if (node->__left)
                next = node->__left;
            else if (node->__right)
                next = node->__right;
            else {
                next = node->__parent;
                if (node->__parent)
                    node->__parent->select_lr(node->is_left_child()) = nullptr;
                node->__delete_node(__alloc);
            }
        }
        __current_size = 0;
    }

    iterator find(_T const& k)
    {
        return iterator(__tree_find(k), this);
    }

    const_iterator find(_T const& k) const
    {
        return const_iterator(__tree_find(k), this);
    }

    template<typename U>
    iterator find(U const& k)
    {
        static_assert(sizeof(typename _Compare::is_transparent) != -1,
                      "C++14 find requires comparator type to have"
                      " is_transparent member type");

        return iterator(const_cast<node_t*>(__tree_find(k)), this);
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
        pair<iterator, bool> __result;

        bool __found_dup;
        node_t *__i = __tree_ins(nullptr, __found_dup, &value, true);

        if (unlikely(__found_dup))
            __result = { iterator(__i, this), false };
        else if (likely(__i != nullptr)) {
            __result = { iterator(__i, this), true };

            TreePolicy::retrace_insert(__root, __i);
        }

        return __result;
    }

    pair<iterator, bool> insert(_T const& __value)
    {
        return emplace(__value);
    }

    template<typename... Args>
    pair<iterator, bool> emplace(Args&& ...__args)
    {
        // Allocate memory for node using allocator
        void* __n_mem = __alloc.allocate(1);

        // Placement new into memory
        node_t *__n = new (__n_mem) node_t;

        // Forwarding placement construct item in-place
        value_type *__ins = new (reinterpret_cast<pointer>(__n->storage.data))
                value_type(std::forward<Args>(__args)...);

        bool __found_dup;
        node_t *__i = __tree_ins(__n, __found_dup, __ins, false);

        if (likely(!__found_dup)) {
            TreePolicy::retrace_insert(__root, __n);
        } else {
            reinterpret_cast<pointer>(__n->storage.data)->~_T();
            __n->~node_t();
            __alloc.deallocate(__n, 1);
        }

        return pair<iterator, bool>(iterator(__i, this), !__found_dup);
    }

    iterator lower_bound(_T const& key) const
    {
        return end();
    }

    //
    // Debugging

    int dump_node(node_t *__node, int __spacing, int __space)
    {
        if (__node == nullptr)
            return __space;

        // Increase distance between levels
        __space += __spacing;

        // Process right child first
        int __right_space = dump_node(__node->__right, __spacing, __space);

        dbgout << "\n";
        for (int i = __spacing; i < __space; i++)
            dbgout << " ";
        dbgout << __node->item() << "\n";

        // Process left child
        int __left_space = dump_node(__node->__left, __spacing, __space);

        return max(__left_space, __right_space);
    }

    void dump(int spacing = 10, int space = 0)
    {
        dbgout << "Dump:\n";
        int __width = dump_node(__root, spacing, space);

        for (int __i = 0; __i < __width; ++__i)
            dbgout << "-";

        dbgout << "\n\n";
    }

    _noinline
    bool validate_failed() const
    {
        cpu_debug_break();
        return false;
    }

    bool validate() const
    {
        iterator __en = end();
        iterator __curr = begin();
        iterator __next = __curr;

        if (__next != __en)
            ++__next;

        size_t __expect_size = 0;

        while (__curr != __en) {
            // Analyze the node
            node_t const *__node = __curr.curr;
            node_t const *__parent = __node->__parent;
            node_t const *__left = __node->__left;
            node_t const *__right = __node->__right;

            if (unlikely(__left && __right && __left == __right)) {
                dbgout << "Parent has both children pointing to same node!\n";
                return validate_failed();
            }

            if (__parent) {
                if (unlikely(__parent->__left != __node &&
                             __parent->__right != __node)) {
                    dbgout << "Parent is not pointing to this node\n";
                    return validate_failed();
                }

                if (unlikely(__parent->__left == __node &&
                             !(__node->item() < __parent->item()))) {
                    dbgout << "Left child is not less than its parent\n";
                    return validate_failed();
                }

                if (unlikely(__parent->__right == __node &&
                             !(__parent->item() < __node->item()))) {
                    dbgout << "Parent is not less than its right child\n";
                    return validate_failed();
                }
            } else if (!__parent && __root != __node) {
                dbgout << "Node has nullptr parent and is not the root\n";
                return validate_failed();
            }

            if (unlikely(__next != __en && !(*__curr < *__next))) {
                dbgout << "Set is not ordered correctly"
                          ", " << *__curr <<
                          " is not less than " <<
                          *__next << "\n";
                return validate_failed();
            }

            ++__expect_size;

            __curr = __next;
            ++__next;
        }

        if (unlikely(__current_size != __expect_size)) {
            dbgout << "Set current size is wrong"
                      ", counted " << __expect_size <<
                      ", object says " << __current_size <<
                      "\n";
            return validate_failed();
        }

        return true;
    }

private:
    void __swap_nodes(node_t *__a, node_t *__b)
    {
        // Handle both cases with one implementation (__a is always parent)
        if (unlikely(__b == __a->__parent)) {
            assert(!"Unlikely eh?");
            std::swap(__a, __b);
        }

        // Find the pointer that points to A
        node_t **a_ptr = likely(__a->__parent)
                ? &__a->select_lr(__a->is_left_child())
                : &__root;

        // Find the pointer that points to B
        node_t **b_ptr = likely(__b->__parent)
                ? &__b->select_lr(__b->is_left_child())
                : &__root;

        if (__a == __b->__parent) {
            //          W           W      //         W           W      //
            // W_child->|           |      //         |           |      //
            //          a           b                 a           b      //
            //         / \   ->    / \     OR        / \   ->    / \     //
            //        b   X       a   X             b   X       a   X    //
            //       / \         / \       //      / \         / \       //
            //      Y   Z       Y   Z      //     Y   Z       Y   Z      //

            dump();

            node_t *__W = __a->__parent;

            node_t *& __W_child = __a->__parent
                    ? __a->__parent->select_lr(__a->is_left_child())
                    : __root;

            bool __b_was_left = __a->__left == __b;
            auto& __a_X = __a->select_lr(!__b_was_left);
            node_t *__X = __a_X;
            node_t *__Y = __b->__left;
            node_t *__Z = __b->__right;

            // From the point of view after the swap
            auto& __b_a = __b->select_lr(__b_was_left);
            auto& __b_X = __b->select_lr(!__b_was_left);

            __b->__parent = __W;
            __a->__parent = __b;

            __a->__left = __Y;
            __a->__right = __Z;

            __b_a = __a;
            __b_X = __X;
            __b->__parent = __W;
            if (__X)
                __X->__parent = __b;
            __W_child = __b;
            if (__Y)
                __Y->__parent = __a;
            if (__Z)
                __Z->__parent = __a;

            dump();
            assert(0);
        } else {
            // Neither is a direct descendent of the other

            // Trade children, parents, balances
            std::swap(__a->__left, __b->__left);
            std::swap(__a->__right, __b->__right);

            // Trade places
            std::swap(*a_ptr, *b_ptr);
        }

        std::swap(__a->__parent, __b->__parent);
        std::swap(__a->__balance, __b->__balance);
    }

    void delete_node(node_t *__node)
    {
        node_t *__parent = __node->__parent;

        while (true) {
            if (__node->__left == nullptr && __node->__right == nullptr) {
                // Easy! No children
                if (__parent != nullptr) {
                    TreePolicy::__retrace_delete(__root, __node);
                    __parent->select_lr(__node->is_left_child()) = nullptr;
                } else {
                    __root = nullptr;
                    assert(__current_size == 1);
                    __current_size = 0;
                    return;
                }
            } else if (__node->__left && !__node->__right) {
                // Easier, has one child so just rewire it so this node's
                // parent just points straight to that child
                TreePolicy::__retrace_delete(__root, __node);
                if (__parent) {
                    __parent->select_lr(__node->is_left_child()) = __node->__left;
                    __node->__left->__parent = __parent;
                } else {
                    __root = __node->__left;
                    __node->__left->__parent = nullptr;
                }
            } else if (__node->__right && !__node->__left) {
                // Mirror image of previous case
                TreePolicy::__retrace_delete(__root, __node);
                if (__parent) {
                    __parent->select_lr(__node->is_left_child()) =
                            __node->__right;
                    __node->__right->__parent = __parent;
                } else {
                    __root = __node->__right;
                    __node->__right->__parent = nullptr;
                }
            } else {
                // Node has two children

                ///                                                    ///
                /// Before                                             ///
                ///                                                    ///
                ///         P* <--&root or &left or &right link        ///
                ///         |                                          ///
                ///         D <-- Deleted                              ///
                ///        / \                                         ///
                ///       L   R <-- right descendent of removed        ///
                ///      .   / \                                       ///
                ///     .   A   .                                      ///
                ///        /     .                                     ///
                ///       S <-- leftmost child of right descendent     ///
                ///        \                                           ///
                ///         C <-- possible right child of replacement  ///
                ///                                                    ///
                /// After                                              ///
                ///                                                    ///
                ///                                                    ///
                ///         P* <--&root or &left or &right link        ///
                ///         |                                          ///
                ///         S <-- leftmost child of right descendent   ///
                ///        / \                                         ///
                ///       L   R <-- right descendent of removed        ///
                ///      .   / \                                       ///
                ///     .   A   .                                      ///
                ///        /     .                                     ///
                ///       C <-- possible right child of replacement    ///
                ///                                                    ///
                /// retrace C ? C : A                                  ///

                // Swap the deleted node with the successor node,
                // then you delete the deleted node which is guaranteed
                // not to have two children now)

                node_t *__R = __node->__right;
                node_t *__S = __R;
                while (__S->__left)
                    __S = __S->__left;
                __swap_nodes(__node, __S);
                // Now start over and it will be easy to delete that node
                continue;
            }

            break;
        }

        __node->__delete_node(__alloc);

        --__current_size;
    }

public:
    iterator erase(const_iterator place)
    {
        node_t *node = place.template ptr<false>();

        iterator result(node, this);
        ++result;

        delete_node(node);

        return result;
    }

    size_type erase(value_type const& key)
    {
        auto it = find(key);

        if (it != end()) {
            erase(it);
            return 1;
        }

        return 0;
    }

    template<typename _K>
    iterator lower_bound(_K const& k)
    {
        node_t *node = __root;

        while (node) {
            auto& item = node->item();

            // diff = k <=> item
            int diff = !_Compare()(k, item) - !_Compare()(item, k);

            if (unlikely(diff == 0))
                break;

            node_t *next_node = node->select_lr(diff < 0);

            if (unlikely(!next_node))
                break;

            node = next_node;
        }

        return iterator(node);
    }

    template<typename _K>
    iterator upper_bound(_K const& key)
    {

    }

private:

    struct node_t {
        node_t *__left = nullptr;
        node_t *__right = nullptr;
        node_t *__parent = nullptr;
        int __balance = 0;
        typename std::aligned_storage<sizeof(_T), alignof(_T)>::type storage;

        void __delete_node(node_allocator_t const& alloc)
        {
            item_ptr()->~value_type();
            alloc.deallocate(this, 1);
        }

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
            return is_left ? __left : __right;
        }

        inline node_t *&select_lr(bool is_left) noexcept
        {
            return is_left ? __left : __right;
        }

        inline bool is_left_child() const noexcept
        {
            return __parent->__left == this;
        }

        inline bool is_right_child() const noexcept
        {
            return __parent->__right == this;
        }
    };

    // Find an existing key, null if no exact match
    template<typename K>
    node_t const *__tree_find(K const& k) const
    {
        node_t const *node = __root;

        while (node) {
            auto& item = node->item();

            // diff = k <=> item
            int diff = !_Compare()(k, item) - !_Compare()(item, k);

            // Break when equal
            if (unlikely(diff == 0))
                break;

            node = node->select_lr(diff < 0);
        }

        return node;
    }

    // Search the tree and find where a node would be inserted,
    // and a flag indicating whether it is


    // Find the insertion point and insert the node
    // Sets found_dup to true if an equal node was found
    // Returns the found existing node or newly inserted node
    template<typename _K, typename... _Args>
    node_t *__tree_ins(node_t *__n, bool &__found_dup,
                       _K const& __key, _Args&& ...__args)
    {
        __found_dup = false;
        auto& lhs = *__key;
        int diff = -1;

        node_t *s;

        // Try hard to avoid doing any memory
        // allocation or copy for failed insert

        if (__root)
        {
            node_t *next = __root;

            do {
                s = next;
                auto&& rhs = s->item();

                // diff = lhs <=> rhs
                diff = !_Compare()(lhs, rhs) - !_Compare()(rhs, lhs);

                if (unlikely(diff == 0))
                {
                    __found_dup = true;
                    return s;
                }

                next = diff < 0 ? s->__left : s->__right;
            } while (next);
        } else {
            // No parent, new node is root
            s = nullptr;
        }

        if (__n == nullptr)
        {
            // Allocate memory for node using allocator
            void* n_mem = __alloc.allocate(1);

            if (!n_mem)
                _OOMPolicy::oom();

            // Placement new into memory
            __n = new (n_mem) node_t;

            // Forwarding placement construct item
            new (reinterpret_cast<pointer>(__n->storage.data))
                    value_type(forward<_Args>(__args)...);
        }

        // New node is initially a leaf
        __n->__left = nullptr;
        __n->__right = nullptr;
        __n->__balance = 0;

        __n->__parent = s;
        ++__current_size;

        if (s)
            s->select_lr(diff < 0) = __n;
        else
            __root = __n;

        return __n;
    }

    using TreePolicy = typename detail::avltree_policy_t<void>::template
        rebind<node_t>;

    node_t *tree_min() const
    {
        node_t *n = __root;

        if (n) {
            while (n->__left)
                n = n->__left;
        }
        return n;
    }

    node_t *tree_max() const
    {
        node_t *n = __root;

        if (n) {
            while (n->__right)
                n = n->__right;
        }
        return n;
    }

private:
    node_t *__root;
    size_t __current_size;
    node_allocator_t __alloc;
};

template<typename T, typename C = less<T>, typename A = allocator<T>>
using set = basic_set<T, C, A>;

__END_NAMESPACE_STD

