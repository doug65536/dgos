#pragma once
#ifdef __DGOS_KERNEL__
#include "types.h"
#else
#include <stdint.h>
#endif
#include "stdlib.h"
#include "mm.h"

#include "memory.h"

__BEGIN_NAMESPACE_EXT

#ifdef __DGOS_KERNEL__

template<typename T>
class free_deleter
{
public:
    constexpr free_deleter() = default;

    _always_inline void operator()(T* __ptr) const
    {
        if (__ptr) {
            destruct(__ptr);
            free(__ptr);
        }
    }

private:
    void destruct(void *) const
    {
    }

    template<typename U>
    void destruct(U *__ptr) const
    {
        __ptr->~U();
    }
};

template<typename _T>
class unique_mmap {
public:
    unique_mmap()
        : __m((_T*)MAP_FAILED)
        , __sz(0)
    {
    }

    unique_mmap(unique_mmap const&) = delete;

    unique_mmap(unique_mmap&& __rhs)
        : __m(__rhs.__m)
        , __sz(__rhs.__sz)
    {
        __rhs.__m = (_T*)MAP_FAILED;
        __rhs.__sz = 0;
    }

    ~unique_mmap()
    {
        reset();
    }

    unique_mmap& operator=(unique_mmap&& __rhs)
    {
        if (this != &__rhs) {
            reset(__rhs.__m, __rhs.__sz);
            __rhs.__m = (_T*)MAP_FAILED;
            __rhs.__sz = 0;
        }
        return *this;
    }

    unique_mmap& operator=(unique_mmap const&) = delete;

    bool mmap(void *__addr, size_t __sz,
              int __prot = PROT_READ | PROT_WRITE,
              int __flags = MAP_UNINITIALIZED,
              int __fd = -1, off_t __ofs = 0)
    {
        reset();

        __m = (_T*)::mmap(__addr, __sz, __prot, __flags, __fd, __ofs);
        if (unlikely(__m == (_T*)MAP_FAILED)) {
            __sz = 0;
            return false;
        }
        this->__sz = __sz;
        return true;
    }

    bool mmap(size_t __sz)
    {
        return mmap(nullptr, __sz);
    }

    // Make the destruct a noop
    void release()
    {
        __m = nullptr;
        __sz = 0;
    }

    void reset(_T *__new_p = (_T*)MAP_FAILED, size_t __new_sz = 0)
    {
        if (__m != MAP_FAILED)
            munmap(__m, __sz);

        __m = __new_p;
        __sz = __new_sz;
    }

    _T* get()
    {
        return __m;
    }

    _T const* get() const
    {
        return __m;
    }

    _T *operator->()
    {
        return __m;
    }

    _T const *operator->() const
    {
        return __m;
    }

    _T& operator*()
    {
        return *__m;
    }

    _T const& operator*() const
    {
        return *__m;
    }

    _T& operator[](size_t __i)
    {
        return __m[__i];
    }

    _T const& operator[](size_t __i) const
    {
        return __m[__i];
    }

    operator _T*()
    {
        return __m;
    }

    operator _T const*() const
    {
        return __m;
    }

    size_t size() const
    {
        return __sz;
    }

private:
    _T *__m;
    size_t __sz;
};

template<typename _T>
using unique_ptr_free = ext::unique_ptr<_T, free_deleter<_T>>;

#endif

__END_NAMESPACE
