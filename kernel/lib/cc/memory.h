#pragma once
#include "types.h"
#include "mm.h"
#include "stdlib.h"

__BEGIN_NAMESPACE_STD

template<typename _T>
struct allocator
{
    typedef _T value_type;

    template<typename _U>
    struct rebind
    {
        using other = allocator<_U>;
    };

    value_type * allocate(size_t __n) const
    {
        return (value_type*)malloc(__n * sizeof(value_type));
    }

    void deallocate(value_type * __p, size_t) const
    {
        free(__p);
    }

    void destruct(value_type * __p) const
    {
        __p->~value_type();
    }
};

struct page_allocator_base
{
protected:
    static void *allocate_impl(size_t n);
    static void deallocate_impl(void *p, size_t n);
};

template<typename _T>
struct page_allocator : public page_allocator_base
{
    typedef _T value_type;

    template<typename _U>
    struct rebind
    {
        using other = allocator<_U>;
    };

    value_type * allocate(size_t __n) const
    {
        return allocate_impl(__n * sizeof(value_type));
    }

    void deallocate(value_type * p, size_t n) const
    {
        deallocate_impl(p, n);
    }
};

__END_NAMESPACE_STD
