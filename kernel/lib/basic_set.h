#pragma once
#include "types.h"
#include "utility.h"
#include "type_traits.h"
#include "functional.h"
#include "numeric_limits.h"
#include "debug.h"
#include "printk.h"
#include "cxxexception.h"

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
class base_tree_policy_t {
public:

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

    static void __swap_nodes(_T *&root, _T *__a, _T *__b)
    {
        assert(__a != __b);

        assert(__a->__parent || root == __a);
        assert(__b->__parent || root == __b);

        // Get the appropriate pointer to the parent's left/right pointer
        // or the tree root pointer

        if (__a->__parent == nullptr) {
            __a->__parent = __b->__parent;
            __b->__parent = nullptr;
        } else if (__b->__parent == nullptr) {
            __b->__parent = __a->__parent;
            __a->__parent = nullptr;
        }

        _T **__a_parent_ptr = __a->__parent
                ? (__a == __a->__parent->__left
                   ? &__a->__parent->__left
                   : &__a->__parent->__right)
                : &root;

        _T **__b_parent_ptr = __b->__parent
                ? (__b == __b->__parent->__left
                   ? &__b->__parent->__left
                   : &__b->__parent->__right)
                : &root;

        assert(*__a_parent_ptr == __a);
        assert(*__b_parent_ptr == __b);

        if (__a->__parent && __b->__parent) {
            std::swap(*__a_parent_ptr, *__b_parent_ptr);
            std::swap(__a->__parent, __b->__parent);
        }
        std::swap(__a->__left, __b->__left);
        std::swap(__a->__right, __b->__right);
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
};

template<typename _T>
class rbtree_policy_t : public base_tree_policy_t<_T>
{
public:
    static constexpr int _RED = 1;
    static constexpr int _BLACK = 0;

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
};

template<typename _T>
class avltree_policy_t
{
public:
    static void __retrace_insert(_T *&root_ptr, _T *_Z)
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
            _X->__balance = -1;   // t23 now higher
            _Z->__balance = +1;   // t4 now lower than X
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

class panic_policy_t
{
public:
    static void *oom()
    {
        panic_oom();
    }
};

}   // namespace detail

template<typename _T, typename _U>
struct __tree_value
{
    using type = std::pair<_T const, _U>;
    using key_type = _T const;
    using value_type = _U;

    _always_inline
    static constexpr _T const& key(type const& __rhs)
    {
        return __rhs.first;
    }

    _always_inline
    static constexpr _U& value(type& __rhs)
    {
        return __rhs.second;
    }

    _always_inline
    static constexpr _U const& value(type const& __rhs)
    {
        return __rhs.second;
    }
};

template<typename _T>
struct __tree_value<_T, void>
{
    using type = _T;
    using key_type = _T;
    using value_type = type;

//    _always_inline
//    static constexpr _T& key(type& __rhs)
//    {
//        return __rhs;
//    }

    _always_inline
    static constexpr _T const& key(type const& __rhs)
    {
        return __rhs;
    }

    _always_inline
    static constexpr _T const& value(type const& __rhs)
    {
        return __rhs;
    }
};

using _TreePolicy = detail::avltree_policy_t<void>;
using _OOMPolicy = detail::panic_policy_t;

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
class __basic_tree_index
{
};

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
class __basic_tree
{
private:
    struct __node_t;

    // Handle void _V
    using __tree_value_t = __tree_value<_T, _V>;
    // Either the pair<<_T,_V>, or just _T if _V is void
    using __item_type = typename __tree_value_t::type;
public:
    using __node_allocator_t =
        typename _Alloc::template rebind<__node_t>::other;

    using key_type = typename __tree_value_t::key_type;
    using mapped_type = typename __tree_value_t::value_type;
    using value_type = typename __tree_value_t::type;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = __item_type*;
    using const_pointer = __item_type const*;
    using reference = __item_type&;
    using const_reference = __item_type const&;
    using node_type = __node_t;
    using node_allocator = __node_allocator_t;

    template<bool _is_const, int dir>
    class __basic_iterator : public bidirectional_iterator_tag
    {
    private:
        static_assert(dir == 1 || -dir == 1, "Unexpected direction");
        using owner_ptr_t = __basic_tree const *;
        using node_ptr_t = __node_t const *;

        template<bool _is_const_inst>
        typename enable_if<!_is_const_inst, __node_t *>::type ptr()
        {
            return const_cast<__node_t*>(__curr);
        }

    public:
        __basic_iterator() = default;
        __basic_iterator(__basic_iterator const& __rhs) = default;
        __basic_iterator &operator=(__basic_iterator const& __rhs) = default;

        template<bool _rhs_is_const, int _dir>
        __basic_iterator(__basic_iterator<_rhs_is_const, _dir> const& __rhs)
            : __curr(__rhs.__curr)
            , __owner(__rhs.__owner)
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
        _always_inline
        __basic_iterator& step(integral_constant<int, 1>::type const&)
        {
            __curr = __tree_next(__curr);

            return *this;
        }

        // Step to the element that compares lesser
        _always_inline
        __basic_iterator& step(integral_constant<int, -1>::type const&)
        {
            __curr = __tree_prev(__curr);

            return *this;
        }

        _always_inline __basic_iterator& inc()
        {
            return step(typename integral_constant<int, dir>::type());
        }

        _always_inline __basic_iterator& dec()
        {
            // Steps to lesser if forward, greater if reverse
            return step(typename integral_constant<int, -dir>::type());
        }

        _always_inline __basic_iterator& operator++()
        {
            return inc();
        }

        _always_inline __basic_iterator& operator--()
        {
            return dec();
        }

        __basic_iterator operator++(int)
        {
            __basic_iterator orig(*this);
            inc();
            return orig;
        }

        __basic_iterator operator--(int)
        {
            __basic_iterator orig(*this);
            dec();
            return orig;
        }

        const_reference operator*() const
        {
            return __curr->__item();
        }

        const_pointer operator->() const
        {
            return __curr->__item_ptr();
        }

        bool operator==(__basic_iterator const& __rhs) const
        {
            return __curr == __rhs.__curr;
        }

        bool operator!=(__basic_iterator const& __rhs) const
        {
            return __curr != __rhs.__curr;
        }

        __basic_iterator operator-(size_type __n) const
        {
            __basic_iterator __result(*this);

            if (__curr == nullptr && __n) {
                --__n;
                if (dir > 0)
                    __result.__curr = __owner->__tree_max();
                else
                    __result.__curr = __owner->__tree_min();
            }

            while (__n-- && __result.__curr)
                --__result;

            return __result;
        }

        __basic_iterator operator+(size_type __n) const
        {
            __basic_iterator __result(*this);
            while (__n-- && __result.__curr)
                ++__result;
            return __result;
        }

    private:
        friend class __basic_tree;

        __basic_iterator(node_ptr_t __node,
                       owner_ptr_t __owner)
            : __curr(__node)
            , __owner(__owner)
        {
        }

        node_ptr_t __curr = nullptr;
        owner_ptr_t __owner = nullptr;
    };

    // Set interestingly only has const iterators
    using const_iterator = __basic_iterator<true, 1>;
    using reverse_iterator = __basic_iterator<true, -1>;
    using iterator = __basic_iterator<false, 1>;
    using const_reverse_iterator = __basic_iterator<false, -1>;

    __basic_tree()
        : __root(nullptr)
        , __current_size(0)
    {
    }

    explicit __basic_tree(_Alloc const& __alloc)
        : __root(nullptr)
        , __current_size(0)
        , __alloc(__alloc)
    {
    }

    explicit __basic_tree(_Compare const& __cmp,
                          _Alloc const& __alloc = _Alloc())
        : __cmp(__cmp)
        , __alloc(__alloc)
    {
    }

    ~__basic_tree()
    {
        clear();
    }

    __basic_tree(__basic_tree const& __rhs)
        : __root(nullptr)
        , __current_size(0)
        , __cmp(__rhs.__cmp)
    {
        for (__item_type const& __item : __rhs)
            insert(__item);
    }

    __basic_tree(__basic_tree&& __rhs)
        : __root(__rhs.__root)
        , __current_size(__rhs.__current_size)
        , __cmp(std::move(__rhs.__cmp))
    {
        __rhs.__root = nullptr;
        __rhs.__current_size = 0;
    }

    __basic_tree& operator=(__basic_tree const& __rhs)
    {
        if (unlikely(&__rhs == this))
            return *this;

        clear();

        for (__item_type const& __item : __rhs)
            insert(__item);

        return *this;
    }

    __basic_tree& operator=(__basic_tree&& __rhs)
    {
        __root = __rhs.__root;
        __rhs.__root = nullptr;

        __current_size = __rhs.__current_size;
        __rhs.__current_size = 0;

        __cmp = std::move(__rhs.__cmp);
        __alloc = std::move(__rhs.__alloc);

        return *this;
    }

    template<typename _K>
    value_type const& operator[](_K const& __key) const
    {
        static_assert(!is_same<_V, void>::value, "Not a map");
        pair<iterator,bool> ins = insert(__key);
        if (likely(ins.first != end()))
            return __tree_value_t::value(*ins.first);
        throw std::bad_alloc();
    }

    template<typename _K>
    value_type& operator[](_K const& __key)
    {
        static_assert(!is_same<_V, void>::value, "Not a map");
        pair<iterator,bool> ins = insert(__key);
        if (likely(ins.first != end()))
            return __tree_value_t::value(*ins.first);
        throw std::bad_alloc();
    }

    template<typename _K>
    value_type& at(_K const& key)
    {
        iterator __it = find(key);
        if (likely(__it != end()))
            return __tree_value_t::value(*__it);
        throw std::out_of_range();
    }

    template<typename _K>
    value_type const& at(_K const& key) const
    {
        const_iterator __it = find(key);
        if (likely(__it != end()))
            return __tree_value_t::value(*__it);
        throw std::out_of_range();
    }

    size_type size() const
    {
        return __current_size;
    }

    bool empty() const
    {
        return __current_size == 0;
    }

    _Alloc get_allocator() const
    {
        return __alloc;
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
        __node_t *__n = __tree_min();
        return iterator(__n, this);
    }

    const_iterator begin() const
    {
        __node_t *__n = __tree_min();
        return const_iterator(__n, this);
    }

    const_iterator cbegin() const
    {
        __node_t *__n = __tree_min();
        return const_iterator(__n, this);
    }

    reverse_iterator rbegin()
    {
        __node_t *__n = __tree_max();
        return reverse_iterator(__n, this);
    }

    const_reverse_iterator rbegin() const
    {
        __node_t *__n = __tree_max();
        return const_reverse_iterator(__n, this);
    }

    const_reverse_iterator crbegin() const
    {
        __node_t *__n = __tree_max();
        return const_reverse_iterator(__n, this);
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
        return std::numeric_limits<size_type>::max() /
                (sizeof(__node_t) + 32);
    }

    void swap(__basic_tree& __rhs)
    {
        std::swap(__root, __rhs.__root);
        std::swap(__current_size, __rhs.__current_size);
        std::swap(__cmp, __rhs.__cmp);
        std::swap(__alloc, __rhs.__alloc);
    }

    void clear()
    {
        for (__node_t *__next, *__node = __root; __node; __node = __next) {
            if (__node->__left)
                __next = __node->__left;
            else if (__node->__right)
                __next = __node->__right;
            else {
                __next = __node->__parent;
                if (__node->__parent)
                    __node->__parent->__select_lr(__node->__is_left_child()) =
                            nullptr;
                __node->__delete_node(__alloc);
            }
        }
        __root = nullptr;
        __current_size = 0;
    }

    iterator find(_T const& __k)
    {
        return iterator(__tree_find(__k), this);
    }

    const_iterator find(_T const& __k) const
    {
        return const_iterator(__tree_find(__k), this);
    }

    template<typename U>
    iterator find(U const& __k)
    {
        static_assert(sizeof(typename _Compare::is_transparent) != -1,
                      "C++14 find requires comparator type to have"
                      " is_transparent member type");

        return iterator(const_cast<__node_t*>(__tree_find(__k)), this);
    }

    template<typename U>
    const_iterator find(U const& __k) const
    {
        static_assert(sizeof(typename _Compare::is_transparent) != -1,
                      "C++14 find requires comparator type to have"
                      " is_transparent member type");

        return const_iterator(__tree_find(__k));
    }

    pair<iterator, bool> insert(__item_type&& __value)
    {
        pair<iterator, bool> __result;

        bool __found_dup;
        __node_t *__i = __tree_ins(nullptr, __found_dup, __value);

        if (unlikely(__found_dup)) {
            __result = { iterator(__i, this), false };
        } else if (likely(__i != nullptr)) {
            new (__i->__storage.__mem.data)
                    __item_type(move(__value));
            __result = { iterator(__i, this), true };

            _NodePolicy::__retrace_insert(__root, __i);
        }

        return __result;
    }

    pair<iterator, bool> insert(__item_type const& __value)
    {
        return emplace(__value);
    }

    template<typename... Args>
    pair<iterator, bool> emplace(Args&& ...__args)
    {
        // Allocate memory for node using allocator
        void* __n_mem = __alloc.allocate(1);

        // Placement new into memory
        // Can't avoid allocating node...
        __node_t *__n = new (__n_mem) __node_t();

        // ...and can't avoid running constructor,
        // because we need a value to pass to the comparator
        __item_type const *__ins =
                new (reinterpret_cast<pointer>(__n->__storage.__mem.data))
                __item_type(std::forward<Args>(__args)...);

        bool __found_dup;
        __node_t *__i = __tree_ins(__n, __found_dup, *__ins);

        if (likely(!__found_dup)) {
            _NodePolicy::__retrace_insert(__root, __n);
        } else {
            reinterpret_cast<pointer>(__n->__storage.__mem.data)->
                    ~__item_type();
            __n->~__node_t();
            __alloc.deallocate(__n, 1);
        }

        return pair<iterator, bool>(iterator(__i, this), !__found_dup);
    }

    iterator lower_bound(_T const& __k) const
    {
        __node_t const *__node;
        int __prev_diff = -10;
        int __diff = -10;
        for (__node = __root; __node; ) {
            auto& __item = __node->__item();

            // diff = k <=> item
            __prev_diff = __diff;
            __diff = !__cmp(__k, __tree_value_t::key(__item)) -
                    !__cmp(__tree_value_t::key(__item), __k);

            // Break when equal
            if (unlikely(__diff == 0))
                break;

            __node_t const *__next = __node->__select_lr(__diff < 0);

            if (!__next)
                break;

            __node = __next;
        }

        if (__node && __diff == -1 && __prev_diff == 1)
            __node = __tree_next(__node);

        return iterator(__node, this);
    }

    //
    // Debugging

    int __dump_node(__node_t *__node, int __spacing, int __space)
    {
        if (__node == nullptr)
            return __space;

        // Increase distance between levels
        __space += __spacing;

        // Process right child first
        int __right_space = __dump_node(__node->__right, __spacing, __space);

        for (int __pass = 0; __pass < 6; ++__pass) {
            for (int i = __spacing; i < __space; i++)
                dbgout << ' ';

            switch (__pass) {
            case 0:
                dbgout << '*' << __node;
                break;
            case 1:
                dbgout << '>' << __node->__right;
                break;
            case 2:
                dbgout << '^' << __node->__parent;
                break;
            case 3:
                dbgout << '<' << __node->__left;
                break;
            case 4:
                dbgout << 'B' << plus << __node->__balance << noplus;
                break;
            case 5:
                dbgout << '=' << __node->__item();
                break;
            }

            dbgout << '\n';
        }
        dbgout << '\n';

        // Process left child
        int __left_space = __dump_node(__node->__left, __spacing, __space);

        return max(__left_space, __right_space);
    }

    void dump(char const *title, int spacing = 10, int space = 0)
    {
        dbgout << "Dump: " << title << '\n';
        int __width = __dump_node(__root, spacing, space);

        for (int __i = 0; __i < __width; ++__i)
            dbgout << '-';

        dbgout << "\n\n";
    }

    void dump(int __spacing = 10, int __space = 0)
    {
        dump("", __spacing, __space);
    }

    _noinline
    bool validate_failed() const
    {
        cpu_debug_break();
        return false;
    }

    bool validate() const
    {
        const_iterator __en = end();
        const_iterator __curr = begin();
        const_iterator __next = __curr;

        if (__next != __en)
            ++__next;

        size_type __expect_size = 0;

        while (__curr != __en) {
            // Analyze the node
            __node_t const *__node = __curr.__curr;
            __node_t const *__parent = __node->__parent;
            __node_t const *__left = __node->__left;
            __node_t const *__right = __node->__right;

            if (unlikely(__left && __right && __left == __right)) {
                dbgout << "Parent has both children pointing to same node!\n";
                return validate_failed();
            }

            if (__parent) {
                if (unlikely(__parent->__left != __node &&
                             __parent->__right != __node)) {
                    dbgout << "Parent is not pointing to " <<
                              (void*)__node << " node\n";
                    return validate_failed();
                }

                if (unlikely(__parent->__left == __node &&
                             !(__cmp(__tree_value_t::key(
                                         __node->__item()),
                                     __tree_value_t::key(
                                         __parent->__item()))))) {
                    dbgout << "Left child is not less than its parent\n";
                    return validate_failed();
                }

                if (unlikely(__parent->__right == __node &&
                             !(__cmp(__tree_value_t::key(
                                         __parent->__item()),
                                     __tree_value_t::key(
                                         __node->__item()))))) {
                    dbgout << "Parent is not less than its right child\n";
                    return validate_failed();
                }
            } else if (!__parent && __root != __node) {
                dbgout << "Node has nullptr parent and is not the root\n";
                return validate_failed();
            } else if (__node->__parent && __node == __root) {
                dbgout << "Root parent pointer is not null\n";
                return validate_failed();
            }

            if (unlikely(__next != __en && !(*__curr < *__next))) {
                dbgout << "Set is not ordered correctly"
                          ", " << *__curr <<
                          " is not less than " <<
                          *__next << '\n';
                return validate_failed();
            }

            if (__node->__balance < 0 && !__node->__left) {
                dbgout << "Balance is wrong?\n";
                return validate_failed();
            }

            if (__node->__balance > 0 && !__node->__right) {
                dbgout << "Balance is wrong?\n";
                return validate_failed();
            }

            if (__node->__balance != 0 &&
                    !__node->__left && !__node->__right) {
                dbgout << "Balance is wrong?\n";
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
    void __swap_nodes(__node_t *__a, __node_t *__b)
    {
        // Handle both cases with one implementation (__a is always parent)
        if (unlikely(__b == __a->__parent)) {
            assert(!"Unlikely eh?");
            std::swap(__a, __b);
        }

        // Find the pointer that points to A
        __node_t **__a_ptr = likely(__a->__parent)
                ? &__a->__parent->__select_lr(__a->__is_left_child())
                : &__root;

        // Find the pointer that points to B
        __node_t **__b_ptr = likely(__b->__parent)
                ? &__b->__parent->__select_lr(__b->__is_left_child())
                : &__root;

        if (__a == __b->__parent) {
            //          W           W      //         W           W      //
            // W_child->|           |      //         |           |      //
            //          a           b                 a           b      //
            //         / \   ->    / \     OR        / \   ->    / \     //
            //        b   X       a   X             X   b       X   a    //
            //       / \         / \       //          / \         / \   //
            //      Y   Z       Y   Z      //         Y   Z       Y   Z  //

            __node_t *__W = __a->__parent;

            __node_t *& __W_child = likely(__W)
                    ? __W->__select_lr(__a->__is_left_child())
                    : __root;

            bool __b_was_left = __a->__left == __b;
            __node_t *& __a_X = __a->__select_lr(!__b_was_left);
            __node_t *__X = __a_X;
            __node_t *__Y = __b->__left;
            __node_t *__Z = __b->__right;

            // From the point of view after the swap
            __node_t *& __b_a = __b->__select_lr(__b_was_left);
            __node_t *& __b_X = __b->__select_lr(!__b_was_left);

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

            //assert(0);
        } else {
            // Neither is a direct descendent of the other

            // Trade children, parents, balances
            std::swap(__a->__left, __b->__left);
            std::swap(__a->__right, __b->__right);
            std::swap(__a->__parent, __b->__parent);
            std::swap(*__a_ptr, *__b_ptr);

            if (__a->__left)
                __a->__left->__parent = __a;
            if (__a->__right)
                __a->__right->__parent = __a;
            if (__b->__left)
                __b->__left->__parent = __b;
            if (__b->__right)
                __b->__right->__parent = __b;
        }

        std::swap(__a->__balance, __b->__balance);
    }

    void __delete_node(__node_t *__node)
    {
        __node_t *__parent = __node->__parent;

        while (true) {
            if (__node->__left == nullptr && __node->__right == nullptr) {
                // Easy! No children
                _NodePolicy::__retrace_delete(__root, __node);
                if (__parent != nullptr) {
                    assert(__parent == __node->__parent);
                    __parent->__select_lr(__node->__is_left_child()) = nullptr;
                } else {
                    __root = nullptr;
                    assert(__current_size == 1);
                    __current_size = 0;
                    return;
                }
            } else if (__node->__left && !__node->__right) {
                // Easier, has one child so just rewire it so this node's
                // parent just points straight to that child
                // Whichever parent points to this node then points
                // to this node's child
                _NodePolicy::__retrace_delete(__root, __node);
                assert(__parent == __node->__parent);
                if (__parent) {
                    __parent->__select_lr(__node->__is_left_child()) =
                            __node->__left;
                    __node->__left->__parent = __parent;
                } else {
                    __root = __node->__left;
                    __node->__left->__parent = nullptr;
                }
            } else if (__node->__right && !__node->__left) {
                // Mirror image of previous case
                _NodePolicy::__retrace_delete(__root, __node);
                assert(__parent == __node->__parent);
                if (__parent) {
                    __parent->__select_lr(__node->__is_left_child()) =
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
                ///       D                                            ///
                ///        \                                           ///
                ///         C <-- possible right child of replacement  ///
                ///                                                    ///
                /// retrace C ? C : A                                  ///

                // Swap the deleted node with the successor node,
                // then you delete the deleted node which is guaranteed
                // not to have two children now)

                __node_t *__R = __node->__right;
                __node_t *__S = __R;
                while (__S->__left)
                    __S = __S->__left;
                __swap_nodes(__node, __S);
                __parent = __node->__parent;
                // Now start over and it will be easy to delete that node
                continue;
            }

            break;
        }

        __node->__delete_node(__alloc);

        --__current_size;
    }

public:
    iterator erase(const_iterator __place)
    {
        __node_t *__node = __place.template ptr<false>();

        iterator __result(__node, this);
        ++__result;

        __delete_node(__node);

        return __result;
    }

    size_type erase(_T const& __key)
    {
        const_iterator __it = find(__key);

        if (__it != end()) {
            erase(__it);
            return 1;
        }

        return 0;
    }

    template<typename _K>
    iterator lower_bound(_K const& k)
    {
        __node_t *__node = __root;

        while (__node) {
            auto& item = __node->__item();

            // diff = k <=> item
            int __diff = !__cmp(k, __tree_value_t::key(item)) -
                    !__cmp(__tree_value_t::key(item), k);

            if (unlikely(__diff == 0))
                break;

            __node_t *__next_node = __node->__select_lr(__diff < 0);

            if (unlikely(!__next_node))
                break;

            __node = __next_node;
        }

        return iterator(__node);
    }

    template<typename _K>
    iterator upper_bound(_K const& __key)
    {

    }

private:

    struct __node_t {
        __node_t *__left = nullptr;
        __node_t *__right = nullptr;
        __node_t *__parent = nullptr;
        int __balance = 0;

        // For debugger
        union __storage_t {
            typename std::aligned_storage<
                sizeof(__item_type), alignof(__item_type)>::type __mem;
            __item_type __instance;

            __storage_t()
            {
            }
        } __storage;

        void __delete_node(__node_allocator_t& __alloc)
        {
            __item_ptr()->~__item_type();
            __alloc.deallocate(this, 1);
        }

        inline reference __item() noexcept
        {
            return *reinterpret_cast<pointer>(__storage.__mem.data);
        }

        inline const_reference __item() const noexcept
        {
            return *reinterpret_cast<const_pointer>(__storage.__mem.data);
        }

        inline pointer __item_ptr() noexcept
        {
            return reinterpret_cast<pointer>(__storage.__mem.data);
        }

        inline const_pointer __item_ptr() const noexcept
        {
            return reinterpret_cast<const_pointer>(__storage.__mem.data);
        }

        inline __node_t * const& __select_lr(bool __is_left) const
        {
            return __is_left ? __left : __right;
        }

        inline __node_t *&__select_lr(bool __is_left) noexcept
        {
            return __is_left ? __left : __right;
        }

        inline bool __is_left_child() const noexcept
        {
            return __parent->__left == this;
        }

        inline bool __is_right_child() const noexcept
        {
            return __parent->__right == this;
        }
    };

    // Find an existing key, null if no exact match
    template<typename _K>
    __node_t const *__tree_find(_K const& __k) const
    {
        __node_t const *__node = __root;

        while (__node) {
            auto& item = __node->__item();

            // diff = k <=> item
            int __diff = !__cmp(__k, __tree_value_t::key(item)) -
                    !__cmp(__tree_value_t::key(item), __k);

            // Break when equal
            if (unlikely(__diff == 0))
                break;

            __node = __node->__select_lr(__diff < 0);
        }

        return __node;
    }

    // Find the insertion point and insert the node
    // Sets found_dup to true if an equal node was found
    // Returns the found existing node or newly inserted node
    // insert: __n is nullptr when the node hasn't been created yet,
    // emplace: __n is the node with the value constructed into it already
    __node_t *__tree_ins(__node_t *__n, bool &__found_dup,
                         __item_type const &__val)
    {
        __found_dup = false;
        int __diff = -1;

        // Assume no parent, and new node is root
        __node_t *__s = nullptr;

        if (__root) {
            __node_t *__next = __root;

            do {
                __s = __next;
                auto const& __rhs = __s->__item();

                // diff = lhs <=> rhs
                __diff = !__cmp(__tree_value_t::key(__val),
                                __tree_value_t::key(__rhs)) -
                        !__cmp(__tree_value_t::key(__rhs),
                               __tree_value_t::key(__val));

                if (unlikely(__diff == 0))
                {
                    __found_dup = true;
                    return __s;
                }

                __next = __diff < 0 ? __s->__left : __s->__right;
            } while (__next);
        }

        if (__n == nullptr) {
            // Allocate memory for node using allocator
            void* __n_mem = __alloc.allocate(1);

            if (unlikely(!__n_mem))
                _OOMPolicy::oom();

            // Placement new into memory
            __n = new (__n_mem) __node_t();

            // Forwarding placement construct item
            new (reinterpret_cast<pointer>(__n->__storage.__mem.data))
                    __item_type(__val);
        }

        // New node is initially a leaf
        __n->__left = nullptr;
        __n->__right = nullptr;
        __n->__balance = 0;

        __n->__parent = __s;
        ++__current_size;

        if (__s)
            __s->__select_lr(__diff < 0) = __n;
        else
            __root = __n;

        return __n;
    }

    using _NodePolicy = typename detail::avltree_policy_t<void>::template
        rebind<__node_t>;

    __node_t *__tree_min() const
    {
        __node_t *__n = __root;

        if (__n) {
            while (__n->__left)
                __n = __n->__left;
        }
        return __n;
    }

    __node_t *__tree_max() const
    {
        __node_t *__n = __root;

        if (__n) {
            while (__n->__right)
                __n = __n->__right;
        }
        return __n;
    }

    static __node_t const *__tree_next(__node_t const *__curr)
    {
        if (__curr) {
            if (__curr->__right) {
                // Find lowest value in right subtree
                __curr = __curr->__right;

                while (__curr->__left)
                    __curr = __curr->__left;
            } else {
                // Go up at least to parent, keep going while from right
                __node_t *p = __curr->__parent;
                while (p && __curr == p->__right) {
                    __curr = p;
                    p = p->__parent;
                }

                __curr = p;
            }
        }

        return __curr;
    }

    static __node_t const *__tree_prev(__node_t const *__curr)
    {
        if (__curr) {
            if (__curr->__left) {
                // Find highest value in left subtree
                __curr = __curr->__left;

                while (__curr->__right)
                    __curr = __curr->__right;
            } else {
                // Go up at least to parent, keep going while from left
                __node_t *p = __curr->__parent;
                while (p && __curr == p->__left) {
                    __curr = p;
                    p = p->__parent;
                }

                __curr = p;
            }
        }

        return __curr;
    }

private:
    __node_t *__root;
    size_type __current_size;
    _Compare __cmp;
    __node_allocator_t __alloc;
};

template<typename _K, typename _V, typename _C>
class __basic_map_comp {
public:
    bool operator()(pair<_K const, _V> const& __lhs,
                    pair<_K const, _V> const& __rhs) const
    {
        return _C()(__lhs.first, __rhs.first);
    }

    template<typename _U,
             typename = typename _U::is_transparent>
    bool operator()(pair<_K const, _V> const& __lhs,
                    _U const& __rhs) const
    {
        return _C()(__lhs.first, __rhs);
    }

    template<typename _U,
             typename = typename _U::is_transparent>
    bool operator()(_U const& __lhs,
                    pair<_K const, _V> const& __rhs)
    {
        return _C()(__lhs, __rhs.first);
    }
};

template<typename _K, typename _C = less<_K>, typename _A = allocator<_K>>
using set = __basic_tree<_K, void, _C, _A>;

template<typename _K, typename _V,
         typename _C = less<_K>, typename _A = allocator<pair<_K const, _V>>>
using map = __basic_tree<_K, _V, _C, _A>;

//template<typename

__END_NAMESPACE_STD

