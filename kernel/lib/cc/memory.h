#pragma once
#include "types.h"
#include "mm.h"
#include "stdlib.h"

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

template<typename _T>
struct page_allocator
{
    typedef _T value_type;

    template<typename _U>
    struct rebind
    {
        using other = allocator<_U>;
    };

    value_type * allocate(size_t __n) const
    {
        return mmap(nullptr, __n * sizeof(value_type),
                    PROT_READ | PROT_WRITE, 0, -1, 0);
    }

    void deallocate(value_type * __p, size_t __n) const
    {
        munmap(__p, __n * sizeof(value_type));
    }
};
