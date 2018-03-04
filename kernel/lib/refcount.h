#pragma once
#include "assert.h"
#include "cpu/atomic.h"

template<typename T>
class refcounted;

template<typename T>
class refptr final {
public:
    refptr()
        : ptr(nullptr)
    {
    }

    refptr(T* rhs)
        : ptr(rhs)
    {
        if (uintptr_t(rhs) > 0x7F)
            rhs->addref();
    }

    refptr(refptr const& rhs)
        : ptr(rhs.ptr)
    {
        if (uintptr_t(rhs.ptr) > 0x7F)
            rhs.ptr->addref();
    }

    refptr(refptr&& rhs)
        : ptr(rhs.ptr)
    {
        rhs.ptr = nullptr;
    }

    refptr& operator=(refptr const& rhs)
    {
        T* old = ptr;

        ptr = rhs.ptr;

        if (uintptr_t(ptr) > 0x7F)
            ptr->addref();

        if (uintptr_t(old) > 0x7F)
            old->releaseref();

        return *this;
    }

    refptr& operator=(T *rhs)
    {
        T *old = ptr;

        ptr = rhs;

        if (uintptr_t(ptr) > 0x7F)
            ptr->addref();

        if (uintptr_t(old) > 0x7F)
            old->releaseref();

        return *this;
    }

    T* detach()
    {
        T* p = ptr;
        ptr = nullptr;
        return p;
    }

    ~refptr()
    {
        if (uintptr_t(ptr) > 0x7F)
            ptr->releaseref();
    }

    __always_inline
    T* get()
    {
        return ptr;
    }

    __always_inline
    T const* get() const
    {
        return ptr;
    }

    __always_inline
    T* operator ->()
    {
        return ptr;
    }

    __always_inline
    T const* operator ->() const
    {
        return ptr;
    }

    __always_inline
    operator bool() const
    {
        return ptr != nullptr;
    }

private:
    T* ptr;
};

template<typename T>
class refcounted {
public:
    __always_inline
    void addref()
    {
        atomic_inc(&refcount);
    }

    void releaseref()
    {
        if (atomic_dec(&refcount) == 0)
            delete static_cast<T*>(this);
    }

protected:
    __always_inline
    refcounted()
        : refcount(0)
    {
    }

    __always_inline
    ~refcounted()
    {
        assert(refcount == 0);
    }

private:
    int mutable refcount;
};
