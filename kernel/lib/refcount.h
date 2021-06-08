#pragma once
#include "assert.h"
#include "atomic.h"
#include "utility.h"

template<typename T, typename C = ext::atomic_int>
class refcounted;

template<typename T>
class refptr final {
public:
    constexpr refptr()
        : ptr(nullptr)
    {
    }

    constexpr refptr(T* rhs)
        : ptr(rhs)
    {
        if (uintptr_t(rhs) > 0x7F)
            rhs->addref();
    }

    constexpr refptr(refptr const& rhs)
        : ptr(rhs.ptr)
    {
        if (uintptr_t(rhs.ptr) > 0x7F)
            rhs.ptr->addref();
    }

    constexpr refptr(refptr&& rhs)
        : ptr(rhs.ptr)
    {
        rhs.ptr = nullptr;
    }

    constexpr refptr& operator=(refptr const& rhs)
    {
        T* old = ptr;

        ptr = rhs.ptr;

        if (uintptr_t(ptr) > 0x7F)
            ptr->addref();

        if (uintptr_t(old) > 0x7F)
            old->releaseref();

        return *this;
    }

    constexpr refptr& operator=(refptr&& rhs)
    {
        if (&rhs != this) {
            T* old = ptr;

            ptr = rhs.ptr;
            rhs.ptr = nullptr;

            if (uintptr_t(old) > 0x7F)
                old->releaseref();
        }
        return *this;
    }

    constexpr refptr& operator=(T *rhs)
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

    _always_inline
    explicit operator T const*() const
    {
        return ptr;
    }

    _always_inline
    explicit operator T*()
    {
        return ptr;
    }

    _always_inline
    T* get()
    {
        return ptr;
    }

    _always_inline
    T const* get() const
    {
        return ptr;
    }

    _always_inline
    T* operator ->()
    {
        return ptr;
    }

    _always_inline
    T const* operator ->() const
    {
        return ptr;
    }

    _always_inline
    operator bool() const
    {
        return ptr != nullptr;
    }

private:
    T* ptr;
};

template<typename T, typename C>
class refcounted {
public:
    _always_inline
    void addref()
    {
        ++refcount;
    }

    void releaseref()
    {
        if (--refcount == 0)
            destroy();
    }

    virtual void destroy()
    {
        delete static_cast<T*>(this);
    }

protected:
    _always_inline
    refcounted()
        : refcount(0)
    {
    }

    _always_inline
    virtual ~refcounted()
    {
        assert(refcount == 0);
    }

private:
    C refcount;
};
