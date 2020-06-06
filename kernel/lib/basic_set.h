#pragma once
#include "types.h"
#include "utility.h"
#include "type_traits.h"
#include "functional.h"
#include "numeric_limits.h"
#include "debug.h"
#include "printk.h"
#include "cxxexception.h"
#include "memory.h"

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
    static _T *__grandparent(_T *__n)
    {
        if (__n && __n->__parent)
            return __n->__parent->__parent;
        return nullptr;
    }

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
        _T *__g = __grandparent(__n);

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

#if 0
template<typename _T>
class rbtree_policy_t : public base_tree_policy_t<_T>
{
public:
    using base = base_tree_policy_t<_T>;

    static constexpr int _RED = 1;
    static constexpr int _BLACK = 0;


    void __rotate_left(_T * __n)
    {
        assert(__n != 0);

        iter_t r = __n->__right;

        __replace_node(__n, r);
        _NODE(__n)->__right = _NODE(r)->__left;

        iter_t rl = _NODE(r)->__left;

        if (rl)
            _NODE(rl)->__parent = __n;

        _NODE(r)->__left = __n;
        _NODE(__n)->__parent = r;
    }

    void __rotate_right(iter_t __n)
    {
        assert(__n != 0);

        iter_t nl = _NODE(__n)->__left;

        __replace_node(__n, nl);
        _NODE(__n)->__left = _NODE(nl)->__right;

        iter_t lr = _NODE(nl)->__right;

        if (lr)
            _NODE(lr)->__parent = __n;

        _NODE(nl)->__right = __n;
        _NODE(__n)->__parent = nl;
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

        _T *__u = base::__uncle(__n);
        _T *__g;

        if (__u && __u->__balance == _RED) {
            __n->__parent->__balance = _BLACK;
            __u->__balance = _BLACK;
            __g = base::__grandparent(__n);
            __g->__balance = _RED;
            __insert_case1(__g);
        } else {
            __insert_case4(__n);
        }
    }

    static void __insert_case4(_T *__n)
    {
        _BASIC_SET_TRACE("Insert case 4\n");

        _T *__g = base::__grandparent(__n);

        if ((__n == __n->__parent->__right &&
             (__n->__parent == __g->__left))) {
            base::__rotate_left(__n->__parent);
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

    template<typename _O>
    static void __retrace_insert(_T *&__root_ptr, _T *_Z, _O *__owner _unused)
    {
        __insert_case1(_Z);
    }

    static void __retrace_delete(_T *&__root_ptr, _T *_N)
    {
        __delete_case1(_N);
    }
};

template<>
class rbtree_policy_t<void>
{
public:
    template<typename U>
    using rebind = rbtree_policy_t<U>;
};
#endif

template<typename _T>
class avltree_policy_t
{
public:
    template<typename _O>
    static void __retrace_insert(_T *&__root_ptr, _T *_Z, _O *__owner _unused)
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
                    if (_Z->__balance > 0) {
                        // Double rotation: Left(Z) then Right(X)

                        _N = __rotate_leftright(_X, _Z);
                    } else                           // Left Left Case
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
                __root_ptr = _N; // N is the new root of the total tree
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
            _X->__balance = 0;
            _Z->__balance = -1;
        } else if (_Y->__balance == 0) {
            _X->__balance = 0;
            _Z->__balance = 0;
        } else {
            // t2 was higher
            _X->__balance = 1;
            // t4 now higher
            _Z->__balance = 0;
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
    using mapped_type = _U;

    _always_inline
    static constexpr key_type const& key(type const& __rhs)
    {
        return __rhs.first;
    }

    _always_inline
    static constexpr mapped_type& value(type& __rhs)
    {
        return __rhs.second;
    }

    _always_inline
    static constexpr mapped_type const& value(type const& __rhs)
    {
        return __rhs.second;
    }

    _always_inline
    static constexpr type with_default_value(key_type const& __rhs)
    {
        return type{__rhs, mapped_type()};
    }

    _always_inline
    static constexpr type with_default_value(key_type&& __rhs)
    {
        return type{move(__rhs), mapped_type()};
    }
};

// Specialization for void value type
template<typename _T>
struct __tree_value<_T, void>
{
    using type = _T;
    using key_type = _T;
    // Maps to itself
    using mapped_type = type;

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

    _always_inline
    static constexpr _T const& with_default_value(type const& __rhs)
    {
        return __rhs;
    }
};

using _TreePolicy = detail::avltree_policy_t<void>;
//using _TreePolicy = detail::rbtree_policy_t<void>;
using _OOMPolicy = detail::panic_policy_t;

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
    using mapped_type = typename __tree_value_t::mapped_type;
    using value_type = typename __tree_value_t::type;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = __item_type*;
    using const_pointer = __item_type const*;
    using reference = __item_type&;
    using const_reference = __item_type const&;
    using node_allocator = __node_allocator_t;

    template<bool _Is_const, int _Dir>
    class __basic_iterator;

    // Some complication to provide node handle with
    // either value(), or, key() and mapped()
    class node_type;

    class node_type_data {
        friend class node_type;
    protected:
        constexpr node_type_data();

        constexpr node_type_data(__node_t *__node,
                                 __node_allocator_t const& __alloc);

        node_type_data(node_type_data const& __rhs) = delete;

        constexpr node_type_data(node_type_data&& __rhs) noexcept;

        ~node_type_data();

        void clear();

        node_type_data& operator=(node_type_data&& __rhs);

        __node_t *__node = nullptr;
        node_allocator __alloc;
    };

    class node_type_set : public node_type_data
    {
    public:
        using value_type = typename __tree_value_t::key_type;

        constexpr node_type_set(__node_t *__node,
                                __node_allocator_t const& __alloc);

        value_type& value() const;
    };

    class node_type_map : public node_type_data
    {
    public:
        using key_type = typename remove_const<
                typename __tree_value_t::key_type>::type;
        using mapped_type = typename __tree_value_t::mapped_type;

        constexpr node_type_map(__node_t *__node,
                                __node_allocator_t const& __alloc);

        key_type& key() const;

        mapped_type& mapped() const;
    };

    class node_type : public conditional<is_same<_V, void>::value,
            node_type_set, node_type_map>::type
    {
        using base = typename conditional<is_same<_V, void>::value,
            node_type_set, node_type_map>::type;

        friend class __basic_tree;
    private:
        __node_t *release();

    public:
        constexpr node_type();

        node_type(node_type&&) = default;

        constexpr node_type(__node_allocator_t const& __alloc);

        constexpr node_type(__node_t *__node,
                            __node_allocator_t const& __alloc);

        ~node_type() = default;

        node_type& operator=(node_type const& __rhs) = delete;

        node_type& operator=(node_type&& __rhs);

        operator bool() const;

        __node_allocator_t& get_allocator();
    };

    using const_iterator = __basic_iterator<true, 1>;
    using const_reverse_iterator = __basic_iterator<true, -1>;
    using reverse_iterator = __basic_iterator<false, -1>;
    using iterator = __basic_iterator<false, 1>;

    template<bool _Is_const, int _Dir>
    class __set_iter_base;

    template<bool _Is_const>
    class __set_iter_base<_Is_const, -1>
            : public bidirectional_iterator_tag
    {
        using __subclass =  typename conditional<_Is_const,
            const_reverse_iterator, reverse_iterator>::type;

        using base_result = typename conditional<_Is_const,
            const_iterator, iterator>::type;

    public:
        base_result base() const
        {
            __subclass __self = *static_cast<__subclass const*>(this);
            return base_result(__tree_next(__self.__curr), __self.__owner);
        }

        base_result current() const
        {
            __subclass __self = *static_cast<__subclass const*>(this);
            return base_result(__self.__curr, __self.__owner);
        }
    };

    template<bool _Is_const>
    class __set_iter_base<_Is_const, 1>
            : public bidirectional_iterator_tag
    {
        // No base method on forward iterators
    };

    template<bool _Is_const, int _Dir>
    class __basic_iterator : public __set_iter_base<_Is_const, _Dir>
    {
    private:
        static_assert(_Dir == 1 || -_Dir == 1, "Unexpected direction");
        using owner_ptr_t = __basic_tree const *;
        using node_ptr_t = typename std::conditional<_Is_const,
            __node_t const *, __node_t *>::type;

        template<bool _is_const_inst>
        typename enable_if<!_is_const_inst, __node_t *>::type ptr()
        {
            return const_cast<__node_t*>(__curr);
        }

    public:
        constexpr __basic_iterator() = default;

        constexpr __basic_iterator(__basic_iterator const& __rhs) = default;

        constexpr __basic_iterator &operator=(
                __basic_iterator const& __rhs) = default;

        template<bool _Rhs_is_const, int _Rhs_Dir>
        constexpr __basic_iterator(
                __basic_iterator<_Rhs_is_const, _Rhs_Dir> const& __rhs)
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

            static_assert(_Is_const || !_Rhs_is_const,
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
            __curr = __owner->__tree_prev(__curr);

            return *this;
        }

        _always_inline __basic_iterator& inc()
        {
            return step(typename integral_constant<int, _Dir>::type());
        }

        _always_inline __basic_iterator& dec()
        {
            // Steps to lesser if forward, greater if reverse
            return step(typename integral_constant<int, -_Dir>::type());
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

        typename std::conditional<_Is_const, const_reference, reference>::type
        operator*()
        {
            return __curr->__item();
        }

        typename std::conditional<_Is_const, const_pointer, pointer>::type
        operator->() const
        {
            return __curr->__item_ptr();
        }

        template<bool _R_is_const>
        bool operator==(__basic_iterator<_R_is_const, _Dir> const& __rhs) const
        {
            return __curr == __rhs.__curr;
        }

        template<bool _R_is_const>
        bool operator!=(__basic_iterator<_R_is_const, _Dir> const& __rhs) const
        {
            return __curr != __rhs.__curr;
        }

        __basic_iterator operator-(size_type __n) const;

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

    constexpr __basic_tree()
        : __root(nullptr)
        , __first(nullptr)
        , __last(nullptr)
        , __current_size(0)
    {
    }

    constexpr explicit __basic_tree(_Alloc const& __alloc)
        : __root(nullptr)
        , __first(nullptr)
        , __last(nullptr)
        , __current_size(0)
        , __alloc(__alloc)
    {
    }

    constexpr explicit __basic_tree(__node_allocator_t&& __alloc)
        : __root(nullptr)
        , __first(nullptr)
        , __last(nullptr)
        , __current_size(0)
        , __alloc(move(__alloc))
    {
    }

    constexpr explicit __basic_tree(_Compare const& __cmp,
                          _Alloc const& __alloc = _Alloc())
        : __root(nullptr)
        , __first(nullptr)
        , __last(nullptr)
        , __current_size(0)
        , __cmp(__cmp)
        , __alloc(__alloc)
    {
    }

    explicit __basic_tree(_Compare const& __cmp,
                          __node_allocator_t&& __other_alloc)
        : __root(nullptr)
        , __first(nullptr)
        , __last(nullptr)
        , __current_size(0)
        , __cmp(__cmp)
        , __alloc(move(__other_alloc))
    {
    }

    __node_allocator_t &share_allocator(__basic_tree const& __other)
    {
        return __alloc = __other.__alloc;
    }

    ~__basic_tree()
    {
        clear();
    }

    constexpr __basic_tree(__basic_tree const& __rhs)
        : __root(nullptr)
        , __first(nullptr)
        , __last(nullptr)
        , __current_size(0)
        , __cmp(__rhs.__cmp)
    {
        for (__item_type const& __item : __rhs)
            insert(__item);
    }

    constexpr __basic_tree(__basic_tree&& __rhs) noexcept
        : __root(__rhs.__root)
        , __first(__rhs.__first)
        , __last(__rhs.__last)
        , __current_size(__rhs.__current_size)
        , __cmp(std::move(__rhs.__cmp))
    {
        __rhs.__root = nullptr;
        __rhs.__first = nullptr;
        __rhs.__last = nullptr;
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

    __basic_tree& operator=(__basic_tree&& __rhs);

    template<typename _K>
    mapped_type& operator[](_K&& __key)
    {
        static_assert(!is_same<_V, void>::value, "Not a map");
        pair<iterator,bool> __ins = __insert_key(move(__key));
        if (likely(__ins.first != end()))
            return __tree_value_t::value(*__ins.first);
        throw std::bad_alloc();
    }

    template<typename _K>
    mapped_type& operator[](_K const& __key)
    {
        static_assert(!is_same<_V, void>::value, "Not a map");
        pair<iterator,bool> __ins = __insert_key(__key);
        if (likely(__ins.first != end()))
            return __tree_value_t::value(*__ins.first);
        throw std::bad_alloc();
    }

    template<typename _K>
    mapped_type& at(_K const& key)
    {
        iterator __it = find(key);
        if (likely(__it != end()))
            return __tree_value_t::value(*__it);
        throw std::out_of_range();
    }

    template<typename _K>
    mapped_type const& at(_K const& key) const
    {
        const_iterator __it = find(key);
        if (likely(__it != end()))
            return __tree_value_t::value(*__it);
        throw std::out_of_range();
    }

    constexpr size_type size() const
    {
        return __current_size;
    }

    constexpr bool empty() const
    {
        return __current_size == 0;
    }

    __node_allocator_t &get_allocator()
    {
        return __alloc;
    }

    ///     (nullptr)    (last)
    ///       rend       rbegin
    ///        |           |
    ///       ~~~+---+---+---+~~~
    ///      :   | V | V | V |   :
    ///       ~~~+---+---+---+~~~
    ///            |           |
    ///          begin        end
    ///         (first)    (nullptr)

    constexpr iterator begin()
    {
        return iterator(__first, this);
    }

    constexpr const_iterator begin() const
    {
        return const_iterator(__first, this);
    }

    constexpr const_iterator cbegin() const
    {
        return const_iterator(__first, this);
    }

    constexpr reverse_iterator rbegin()
    {
        return reverse_iterator(__last, this);
    }

    constexpr const_reverse_iterator rbegin() const
    {
        return const_reverse_iterator(__last, this);
    }

    constexpr const_reverse_iterator crbegin() const
    {
        return const_reverse_iterator(__last, this);
    }

    _const
    constexpr iterator end()
    {
        return iterator(nullptr, this);
    }

    _const
    constexpr const_iterator end() const
    {
        return const_iterator(nullptr, this);
    }

    _const
    constexpr const_iterator cend() const
    {
        return const_iterator(nullptr, this);
    }

    _const
    constexpr reverse_iterator rend()
    {
        return reverse_iterator(nullptr, this);
    }

    _const
    constexpr const_reverse_iterator rend() const
    {
        return const_reverse_iterator(nullptr, this);
    }

    _const
    constexpr const_reverse_iterator crend() const
    {
        return const_reverse_iterator(nullptr, this);
    }

    constexpr size_type max_size()
    {
        return std::numeric_limits<size_type>::max() /
                (sizeof(__node_t) + 32);
    }

    void swap(__basic_tree& __rhs)
    {
        std::swap(__root, __rhs.__root);
        std::swap(__first, __rhs.__first);
        std::swap(__last, __rhs.__last);
        std::swap(__current_size, __rhs.__current_size);
        std::swap(__cmp, __rhs.__cmp);
        std::swap(__alloc, __rhs.__alloc);
    }

    void clear();

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

    template<typename _InputIt>
    void insert(_InputIt __st, _InputIt __en)
    {
        while (__st != __en) {
            auto item = *__st;
            insert(item);
            ++__st;
        }
    }

    pair<iterator, bool> insert(__item_type&& __value);

    pair<iterator, bool> __insert_key(key_type const& __key);

    pair<iterator, bool> __insert_key(key_type&& __key);

    pair<iterator, bool> insert(__item_type const& __value);

    pair<iterator, bool> insert(node_type&& __node_handle);

    template<typename... Args>
    pair<iterator, bool> emplace(Args&& ...__args);

    template<typename _KT>
    iterator __lower_bound_impl(_KT const& __k, bool go_next) const;

    iterator lower_bound(_T const& __k) const
    {
        return __lower_bound_impl(__k, true);
    }

    template<typename _K>
    iterator lower_bound(_K const& __k) const
    {
        return __lower_bound_impl(__k, true);
    }

    //
    // Debugging

    int __dump_node(__node_t *__node, int __spacing, int __space) const;

    void dump(char const *__title, int __spacing = 10, int __space = 0) const;

    void dump(int __spacing = 10, int __space = 0)
    {
        dump("", __spacing, __space);
    }

    _noinline
    bool validate_failed() const
    {
        dump("validate failed\n");
        cpu_debug_break();
        return false;
    }

    bool validate() const;

private:
    void __swap_nodes(__node_t *__a, __node_t *__b);

    void __remove_node(__node_t *__node);

    iterator __extract_node(const_iterator __place, bool __destroy);

public:
    size_type erase(_T const& __key)
    {
        const_iterator __it = find(__key);

        if (__it != end()) {
            erase(__it);
            return 1;
        }

        return 0;
    }

    iterator erase(const_iterator __place)
    {
        return __extract_node(__place, true);
    }

    node_type extract(const_iterator __place)
    {
        __extract_node(__place, false);
        __node_t *__node = __place.template ptr<false>();
        return node_type(__node, __alloc);
    }

    node_type extract(_T const& __key)
    {
        const_iterator __it = find(__key);

        if (likely(__it != end()))
            return extract(__it);

        return node_type(nullptr, __alloc);
    }

//    template<typename _K>
//    iterator upper_bound(_K const& __key)
//    {
//    }

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

            ~__storage_t()
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
    __node_t const *__tree_find(_K const& __k) const;

    // Find an existing key, null if no exact match
    template<typename _K>
    __node_t *__tree_find(_K const& __k)
    {
        return const_cast<__node_t*>(static_cast<__basic_tree const*>(this)->
                                     __tree_find(__k));
    }

    // Find the insertion point and insert the node
    // Sets found_dup to true if an equal node was found
    // Returns the found existing node or newly inserted node
    // insert: __n is nullptr when the node hasn't been created yet,
    // emplace: __n is the node with the value constructed into it already
    __node_t *__tree_ins(__node_t *__n, bool &__found_dup,
                         key_type const &__key);

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

    static __node_t *__tree_next(__node_t *__curr)
    {
        return const_cast<__node_t*>(
                    __tree_next((__node_t const *)__curr));
    }

    __node_t const *__tree_prev(__node_t const *__curr) const
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
        } else {
            __curr = __last;
        }

        return __curr;
    }

    __node_t *__tree_prev(__node_t *__curr) const
    {
        return const_cast<__node_t*>(__tree_prev((__node_t const *)__curr));
    }

private:
    __node_t *__root;
    __node_t *__first;
    __node_t *__last;
    size_type __current_size;
    _Compare __cmp;
    __node_allocator_t __alloc;
};

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_data::node_type_data()
    : __node(nullptr)
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_data::node_type_data(
        __node_t *__node, __node_allocator_t const& __alloc)
    : __node(__node)
    , __alloc(__alloc)
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_data::node_type_data(
        node_type_data&& __rhs) noexcept
    : __node(__rhs.__node)
    , __alloc(move(__rhs.__alloc))
{
    __rhs.__node = nullptr;
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_data::~node_type_data()
{
    clear();
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
void __basic_tree<_T, _V, _Compare, _Alloc>::node_type_data::clear()
{
    if (unlikely(__node)) {
        __node->__delete_node(__alloc);
        __node = nullptr;
    }
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::node_type_data&
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_data::operator=(
        node_type_data&& __rhs)
{
    clear();
    __node = __rhs.__node;
    __rhs.__node = nullptr;
    __alloc = move(__rhs.__alloc);
    return *this;
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_set::node_type_set(
        __node_t *__node, __node_allocator_t const& __alloc)
    : node_type_data(__node, __alloc)
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::node_type_set::value_type&
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_set::value() const
{
    return node_type_data::__node->__item();
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_map::node_type_map(
        __node_t *__node, __node_allocator_t const& __alloc)
    : node_type_data(__node, __alloc)
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::node_type_map::key_type&
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_map::key() const
{
    return const_cast<key_type&>(
                __tree_value_t::key(
                    node_type_data::__node->__item()));
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::node_type_map::mapped_type&
__basic_tree<_T, _V, _Compare, _Alloc>::node_type_map::mapped() const
{
    return const_cast<mapped_type&>(
                __tree_value_t::value(
                    node_type_data::__node->__item()));
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::__node_t *
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::release()
{
    __node_t *__result = this->__node;
    this->__node = nullptr;
    return __result;
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::node_type()
    : base(nullptr, __node_allocator_t())
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::node_type(
        __node_allocator_t const& __alloc)
    : base(nullptr, __alloc)
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
constexpr
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::node_type(
        __node_t *__node, __node_allocator_t const& __alloc)
    : base(__node, __alloc)
{
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::node_type::node_type&
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::operator=(node_type&& __rhs)
{
    if (this->__node && this->__node != __rhs.__node)
        node_type_data::clear();
    this->__node = __rhs.__node;
    this->__alloc = __rhs.__alloc;
    __rhs.__node = nullptr;
    return *this;
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::operator bool() const
{
    return node_type_data::__node != nullptr;
}

template<
    typename _T,
    typename _V,
    typename _Compare = less<_T>,
    typename _Alloc = allocator<void>>
typename __basic_tree<_T, _V, _Compare, _Alloc>::__node_allocator_t&
__basic_tree<_T, _V, _Compare, _Alloc>::node_type::get_allocator()
{
    return this->__alloc;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
template<bool _Is_const, int _Dir>
typename __basic_tree<_T, _V, _Compare, _Alloc>::
template __basic_iterator<_Is_const, _Dir>
__basic_tree<_T, _V, _Compare, _Alloc>::__basic_iterator<_Is_const, _Dir>::
operator-(size_type __n) const
{
    __basic_iterator __result(*this);

    if (__curr == nullptr && __n) {
        --__n;
        __result.__curr = (_Dir > 0)
                ? __owner->__last
                : __owner->__first;
    }

    while (__n-- && __result.__curr)
        --__result;

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
__basic_tree<_T, _V, _Compare, _Alloc> &
__basic_tree<_T, _V, _Compare, _Alloc>::operator=(__basic_tree &&__rhs)
{
    __root = __rhs.__root;
    __rhs.__root = nullptr;

    __first = __rhs.__first;
    __rhs.__first = nullptr;

    __last = __rhs.__last;
    __rhs.__last = nullptr;

    __current_size = __rhs.__current_size;
    __rhs.__current_size = 0;

    __cmp = std::move(__rhs.__cmp);
    __alloc = std::move(__rhs.__alloc);

    return *this;
}


template<typename _T, typename _V, typename _Compare, typename _Alloc>
void __basic_tree<_T, _V, _Compare, _Alloc>::clear()
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
    __first = nullptr;
    __last = nullptr;
    __current_size = 0;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
pair<typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator, bool>
__basic_tree<_T, _V, _Compare, _Alloc>::insert(
        __basic_tree::__item_type &&__value)
{
    pair<iterator, bool> __result;

    bool __found_dup;
    __node_t *__ins = __tree_ins(nullptr, __found_dup,
                                 __tree_value_t::key(__value));

    if (unlikely(__found_dup)) {
        __result = { iterator(__ins, this), false };
    } else if (likely(__ins != nullptr)) {
        new (__ins->__storage.__mem.data)
                __item_type(move(__value));
        __result = { iterator(__ins, this), true };

        _NodePolicy::__retrace_insert(__root, __ins, this);
    }

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
pair<typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator, bool>
__basic_tree<_T, _V, _Compare, _Alloc>::__insert_key(key_type const& __key)
{
    pair<iterator, bool> __result;

    bool __found_dup;
    __node_t *__ins = __tree_ins(nullptr, __found_dup, __key);

    if (unlikely(__found_dup)) {
        __result = { iterator(__ins, this), false };
    } else if (likely(__ins != nullptr)) {
        new (__ins->__storage.__mem.data)
                __item_type(__tree_value_t::with_default_value(__key));
        __result = { iterator(__ins, this), true };

        _NodePolicy::__retrace_insert(__root, __ins, this);
    }

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
pair<typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator, bool>
__basic_tree<_T, _V, _Compare, _Alloc>::__insert_key(key_type&& __key)
{
    pair<iterator, bool> __result;

    bool __found_dup;
    __node_t *__ins = __tree_ins(nullptr, __found_dup, __key);

    if (unlikely(__found_dup)) {
        __result = { iterator(__ins, this), false };
    } else if (likely(__ins != nullptr)) {
        new (__ins->__storage.__mem.data)
                __item_type(__tree_value_t::with_default_value(
                                move(__key)));
        __result = { iterator(__ins, this), true };

        _NodePolicy::__retrace_insert(__root, __ins, this);
    }

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
pair<typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator, bool>
__basic_tree<_T, _V, _Compare, _Alloc>::insert(__item_type const& __value)
{
    pair<iterator, bool> __result;

    bool __found_dup;
    __node_t *__ins = __tree_ins(nullptr, __found_dup,
                                 __tree_value_t::key(__value));

    if (unlikely(__found_dup)) {
        __result = { iterator(__ins, this), false };
    } else if (likely(__ins != nullptr)) {
        new (__ins->__storage.__mem.data)
                __item_type(__value);
        __result = { iterator(__ins, this), true };

        _NodePolicy::__retrace_insert(__root, __ins, this);
    }

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
pair<typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator, bool>
__basic_tree<_T, _V, _Compare, _Alloc>::insert(node_type&& __node_handle)
{
    pair<iterator, bool> __result;

    if (likely(__node_handle)) {
        __node_t *__node = __node_handle.release();

        bool __found_dup;
        __node_t *__ins = __tree_ins(__node, __found_dup,
                                     __node->__item());

        if (unlikely(__found_dup)) {
            __result = { iterator(__ins, this), false };
        } else if (likely(__node != nullptr)) {
            __result = { iterator(__node, this), true };

            _NodePolicy::__retrace_insert(__root, __node, this);
        }
    }

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
template<typename... Args>
pair<typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator, bool>
__basic_tree<_T, _V, _Compare, _Alloc>::emplace(Args&& ...__args)
{
    // Allocate memory for node using allocator
    void* __n_mem = __alloc.allocate(1);

    // Placement new into memory
    // Can't avoid allocating node...
    __node_t *__n = new (__n_mem) __node_t();

    // ...and can't avoid running constructor,
    // because we need a value to pass to the comparator
    __item_type const *__item =
            new (reinterpret_cast<pointer>(__n->__storage.__mem.data))
            __item_type(forward<Args>(__args)...);

    bool __found_dup;
    __node_t *__ins = __tree_ins(__n, __found_dup,
                                 __tree_value_t::key(*__item));

    if (likely(!__found_dup)) {
        _NodePolicy::__retrace_insert(__root, __n, this);
    } else {
        reinterpret_cast<pointer>(__n->__storage.__mem.data)->
                ~__item_type();
        __n->~__node_t();
        __alloc.deallocate(__n, 1);
        __n = nullptr;
    }

    return pair<iterator, bool>(iterator(__ins, this), !__found_dup);
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
template<typename _KT>
typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator
__basic_tree<_T, _V, _Compare, _Alloc>::__lower_bound_impl(
        _KT const& __k, bool go_next) const
{
    __node_t *__node;
    int __diff = 0;
    for (__node = __root; __node; ) {
        auto& __item = __node->__item();

        // diff = k <=> item
        __diff = !__cmp(__k, __tree_value_t::key(__item)) -
                !__cmp(__tree_value_t::key(__item), __k);

        // Break when equal
        if (unlikely(__diff == 0))
            break;

        __node_t *__next = __node->__select_lr(__diff < 0);

        if (!__next)
            break;

        __node = __next;
    }

    if (go_next) {
        if (__node && __diff > 0)
            __node = __tree_next(__node);
    } else {
        if (__node && __diff < 0)
            __node = __node ? __tree_prev(__node) : __last;
    }

    return iterator(__node, this);
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
int __basic_tree<_T, _V, _Compare, _Alloc>::__dump_node(
        __node_t *__node, int __spacing, int __space) const
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
            dbgout << 'B' << dec << plus << __node->__balance << noplus;
            break;
        case 5:
            dbgout << '=' << hex << __node->__item();
            break;
        }

        dbgout << '\n';
    }
    dbgout << '\n';

    // Process left child
    int __left_space = __dump_node(__node->__left, __spacing, __space);

    return max(__left_space, __right_space);
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
void __basic_tree<_T, _V, _Compare, _Alloc>::dump(
        char const *__title, int __spacing, int __space) const
{
    dbgout << "Dump: " << __title << '\n';
    int __width = __dump_node(__root, __spacing, __space);

    for (int __i = 0; __i < __width; ++__i)
        dbgout << '-';

    dbgout << "\n\n";
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
bool __basic_tree<_T, _V, _Compare, _Alloc>::validate() const
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

    __node_t *__actual_min = __tree_min();
    __node_t *__actual_max = __tree_max();

    if (unlikely(__first != __actual_min)) {
        dbgout << "__first is wrong"
                  ", expected " << __expect_size <<
                  ", object says " << __current_size <<
                  "\n";
        return validate_failed();
    }

    if (unlikely(__last != __actual_max)) {
        dbgout << "__last is wrong"
                  ", expected " << __expect_size <<
                  ", object says " << __current_size <<
                  "\n";
        return validate_failed();
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

template<typename _T, typename _V, typename _Compare, typename _Alloc>
void __basic_tree<_T, _V, _Compare, _Alloc>::__swap_nodes(
        __node_t *__a, __node_t *__b)
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

template<typename _T, typename _V, typename _Compare, typename _Alloc>
void __basic_tree<_T, _V, _Compare, _Alloc>::__remove_node(__node_t *__node)
{
    __node_t *__parent = __node->__parent;

    if (__node->__left && __node->__right) {
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

        //dump("before swap");

        __node_t *__R = __node->__right;
        __node_t *__S = __R;
        while (__S->__left)
            __S = __S->__left;
        __swap_nodes(__node, __S);
        __parent = __node->__parent;

        //dump("after swap");

        // Now fall through, it is one of the following cases now
    }

    if (__node->__left && !__node->__right) {
        // Easier, has one child so just rewire it so this node's
        // parent just points straight to that child
        // Whichever parent points to this node then points
        // to this node's child
        _NodePolicy::__retrace_delete(__root, __node->__left);
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
        _NodePolicy::__retrace_delete(__root, __node->__right);
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
        // Easy! No children
        _NodePolicy::__retrace_delete(__root, __node);
        if (__parent != nullptr) {
            assert(__parent == __node->__parent);
            __parent->__select_lr(__node->__is_left_child()) = nullptr;
        } else {
            __root = nullptr;
            __first = nullptr;
            __last = nullptr;
            assert(__current_size == 1);
        }
    }

    --__current_size;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
typename __basic_tree<_T, _V, _Compare, _Alloc>::iterator
__basic_tree<_T, _V, _Compare, _Alloc>::__extract_node(
        const_iterator __place, bool __destroy)
{
    assert(__place.__owner == this);

    assert(__first && __last);

    __node_t *__node = __place.template ptr<false>();

    iterator __result(__node, this);
    ++__result;

    __node_t *__new_first = __first;
    __node_t *__new_last = __last;

    if (__first == __last) {
        // Deleting only node
        assert(__node == __first);
        __new_first = nullptr;
        __new_last = nullptr;
    } else if (__first == __node) {
        // Deleting first node
        __new_first = __result.__curr;
    } else if (__last == __node) {
        __new_last = __tree_prev(__last);
    }

    __first = __new_first;
    __last = __new_last;

    __remove_node(__node);

    if (__destroy) {
        __alloc.deallocate(__node, 1);
    } else {
        __node->__parent = nullptr;
        __node->__left = nullptr;
        __node->__right = nullptr;
        __node->__balance = 0;
    }

    return __result;
}

template<typename _T, typename _V, typename _Compare, typename _Alloc>
template<typename _K>
typename __basic_tree<_T, _V, _Compare, _Alloc>::__node_t const *
__basic_tree<_T, _V, _Compare, _Alloc>::__tree_find(_K const& __k) const
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

template<typename _T, typename _V, typename _Compare, typename _Alloc>
typename __basic_tree<_T, _V, _Compare, _Alloc>::__node_t *
__basic_tree<_T, _V, _Compare, _Alloc>::__tree_ins(
        __node_t *__n, bool &__found_dup, key_type const &__key)
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
            __diff = !__cmp(__key, __tree_value_t::key(__rhs)) -
                    !__cmp(__tree_value_t::key(__rhs), __key);

            __next = __diff < 0 ? __s->__left : __s->__right;
        } while (__diff && __next);

        if (__diff == 0) {
            __found_dup = true;
            return __s;
        }
    }

    if (__n == nullptr) {
        // Allocate memory for node using allocator
        void* __n_mem = __alloc.allocate(1);

        if (unlikely(!__n_mem))
            _OOMPolicy::oom();

        // Placement new into memory
        __n = new (__n_mem) __node_t();
    }

    // New node is initially a leaf
    __n->__left = nullptr;
    __n->__right = nullptr;
    __n->__balance = 0;

    __n->__parent = __s;
    ++__current_size;

    if (!__root || (__n->__parent == __first && __diff < 0))
        __first = __n;
    if (!__root || (__n->__parent == __last && __diff > 0))
        __last = __n;

    if (__s)
        __s->__select_lr(__diff < 0) = __n;
    else
        __root = __n;

    return __n;
}



template<typename _K, typename _C = less<void>, typename _A = allocator<_K>>
using set = __basic_tree<_K, void, _C, _A>;

template<typename _K, typename _V,
         typename _C = less<void>, typename _A = allocator<pair<_K const, _V>>>
using map = __basic_tree<_K, _V, _C, _A>;

//template<typename

__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT

using fast_tree_alloc_t = ext::bump_allocator<void, ext::page_allocator<char>>;

template<typename _T, typename _C = std::less<_T>>
using fast_set = std::set<_T, _C, fast_tree_alloc_t>;

template<typename _K, typename _V, typename _C = std::less<_K>>
using fast_map = std::map<_K, _V, _C, fast_tree_alloc_t>;

__END_NAMESPACE_EXT
