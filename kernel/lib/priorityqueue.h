#pragma once
#include "assert.h"
#include "stdlib.h"
#include "utility.h"

template<typename T>
class priqueue_t {
public:
    typedef T value_type;

    typedef int (*priqueue_comparator_t)(T const& lhs, T const &rhs, void *ctx);
    typedef void (*priqueue_swapped_t)(T const& a, T const& b, void *ctx);

    priqueue_t(priqueue_comparator_t cmp,
               priqueue_swapped_t swapped, void *ctx,
               uint32_t init_capacity = 0);
    ~priqueue_t();

    value_type const& peek();
    void push(value_type item);
    value_type pop();

    size_t update(size_t index);
    void remove_at(size_t index);

    size_t size();

private:
    inline T& item(size_t index)
    {
        return items[index];
    }

    int grow();
    inline size_t leftchild(size_t index);
    inline size_t parent(size_t index);
    inline size_t swap(size_t a, size_t b);
    size_t siftup(size_t index);
    size_t siftdown(size_t index);

    value_type *items;
    uint32_t capacity;
    uint32_t count;
    priqueue_comparator_t cmp;
    priqueue_swapped_t swapped;
    void *ctx;
    void *align[3];
};

template<typename T>
priqueue_t<T>::priqueue_t(priqueue_comparator_t init_cmp,
                          priqueue_swapped_t init_swapped,
                          void *init_ctx,
                          uint32_t init_capacity)
    : capacity(init_capacity)
    , count(0)
    , cmp(init_cmp)
    , swapped(init_swapped)
    , ctx(init_ctx)
    , align{0}
{
    if (init_capacity == 0)
        capacity = (PAGE_SIZE - _MALLOC_OVERHEAD) / sizeof(value_type);

    items = (value_type*)malloc(capacity * sizeof(value_type));
}

template<typename T>
priqueue_t<T>::~priqueue_t()
{
    free(items);
}

template<typename T>
int priqueue_t<T>::grow()
{
    // not implemented (yet)
    return 0;
}

template<typename T>
inline size_t priqueue_t<T>::leftchild(size_t index)
{
    return (index + index) + 1;
}

template<typename T>
inline size_t priqueue_t<T>::parent(size_t index)
{
    return (index - 1) >> 1;
}

template<typename T>
inline size_t priqueue_t<T>::swap(size_t a, size_t b)
{
    value_type &ap = item(a);
    value_type &bp = item(b);

    ::swap(ap, bp);

    if (swapped)
        swapped(ap, bp, ctx);

    return a;
}

template<typename T>
size_t priqueue_t<T>::siftup(size_t index)
{
    while (index != 0) {
        size_t parent_index = parent(index);
        int cmp_result = cmp(item(parent_index), item(index), ctx);
        if (cmp_result >= 0)
            break;
        index = swap(parent_index, index);
    }
    return index;
}

template<typename T>
size_t priqueue_t<T>::siftdown(size_t index)
{
    assert(count > 0);

    for (;;) {
        size_t child = leftchild(index);
        if (child < count) {
            value_type &l_val = item(child);
            value_type &r_val = item(child + 1);
            int cmp_result = cmp(l_val, r_val, ctx);
            // Swap with right child if right child is smaller
            child += (cmp_result > 0);
            value_type& this_item = item(index);
            value_type& child_item = item(child);
            cmp_result = cmp(this_item, child_item, ctx);
            if (cmp_result > 0) {
                index = swap(child, index);
                continue;
            }
        }
        return index;
    }
}

template<typename T>
T const& priqueue_t<T>::peek()
{
    assert(count > 0);
    return item(0);
}

template<typename T>
void priqueue_t<T>::push(T new_item)
{
    if (unlikely(count >= capacity))
        grow();

    size_t index = count++;
    item(index) = move(new_item);
    siftup(index);
}

template<typename T>
T priqueue_t<T>::pop()
{
    assert(count > 0);

    value_type &top = item(0);
    value_type &last = item(count - 1);
    value_type item = move(top);
    top = last;
    if (--count > 0)
        siftdown(0);
    return item;
}

template<typename T>
size_t priqueue_t<T>::update(size_t index)
{
    if (assert(index < count)) {
        index = siftup(index);
        index = siftdown(index);
    }
    return index;
}

template<typename T>
void priqueue_t<T>::remove_at(size_t index)
{
    if (assert(index < count)) {
        value_type &top = item(index);
        value_type &last = item(count - 1);
        top = last;
        --count;
        update(index);
    }
}

template<typename T>
size_t priqueue_t<T>::size()
{
    return count;
}


class priqueue_test_t {
public:
    void test();
    static int priqueue_cmp(int const &a, int const &b, void *);
    static void priqueue_swapped(int const&, int const&, void *);
};

extern priqueue_test_t priqueue_test;
