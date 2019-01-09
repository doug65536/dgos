#pragma once

#ifdef __DGOS_KERNEL__
#include "types.h"
#else
#include <stdint.h>
#endif

#ifdef __DGOS_KERNEL__
#include "mm.h"
#endif
#include "stdlib.h"

#ifdef __DGOS_KERNEL__
#include "refcount.h"
#endif

#include "memory.h"

__BEGIN_NAMESPACE_STD

template<typename _T>
struct allocator
{
    using value_type = _T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = _T&;
    using const_reference = _T const&;
    using pointer = _T*;
    using const_pointer = _T const*;

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

template<>
struct allocator<void>
{
    using value_type = void;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = void*;
    using const_pointer = void const*;

    template<typename _U>
    struct rebind
    {
        using other = allocator<_U>;
    };
};

#ifdef __DGOS_KERNEL__

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

template<typename T, typename Alloc = allocator<void>,
         size_t chunk_size = PAGESIZE << 4>
class bump_allocator_impl
    : public refcounted<T>
{
private:
    struct freetag_t {};
    struct emplace_tag_t {};

    union item_t
    {
        // Either a data item...
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;

        // Or a free chain pointer
        item_t *next_free;
    };

    static constexpr size_t page_capacity =
            (chunk_size - sizeof(void*)) / sizeof(T);

public:
    bump_allocator_impl()
    {
        current_page = nullptr;
        bump_index = page_capacity;
        first_free = nullptr;
        last_free = nullptr;
    }

    T *allocate(size_t __n)
    {
        assert(__n < page_capacity);

        size_t remain = page_capacity - bump_index;

        // Fastpath allocation when it would fit within capacity
        if (remain >= __n) {
            T *result = reinterpret_cast<T*>(
                        current_page[bump_index].storage.data);
            bump_index += __n;
            return result;
        }

        // Reuse block if possible
        if (first_free)
        {
            T *result = reinterpret_cast<T*>(first_free->storage.data);

            // Advance first_free to next item
            first_free = first_free->next_free;

            // Null out last_free if first_free became null
            last_free = first_free ? last_free : nullptr;

            return result;
        }

        if (likely(bump_index < page_capacity))
            return reinterpret_cast<T*>(
                        current_page[bump_index++].storage.data);

        // Page full or first allocation ever

        // Link new page back to this page
        char *prev_page = current_page;

        // Allocate a new page
        current_page = alloc.allocate(chunk_size);

        // Pointer sized bytes at the end of the page
        // are reserved for previous page chain
        *(void**)((char*)current_page + chunk_size - sizeof(void*)) = prev_page;

        // Reset index to start of freshly allocated page
        bump_index = 0;

        return reinterpret_cast<T*>(
                    current_page[bump_index++].storage.data);
    }

    void deallocate(T *__p, size_t __n)
    {
        // Either write next pointer into last block, or,
        // write that into first_free instead
        void **adj = last_free ? (void**)last_free : &first_free;
        *adj = __p;

        // Newly deallocated block is reused last
        // after all previously freed blocks
        // This should reduce temporal locality but increase spacial locality
        last_free = __p;

        // Null out next pointer in free block chain
        *(void**)__p = nullptr;
    }

private:
    item_t *current_page;
    size_t bump_index;

    // Rebind incoming allocator to allocate item_t objects
    typename Alloc::template rebind<char>::other alloc;

    item_t *first_free;
    item_t *last_free;
};

// The stateful allocator object is just a refcounted pointer to
// an underlying (and shared by copies) bump_allocator_impl
template<typename _T, typename _Alloc>
class bump_allocator
{
public:
    typedef _T value_type;

    static_assert(sizeof(_T) >= sizeof(void*),
                  "Type is too small to bump allocate");

    template<typename _U>
    struct rebind
    {
        using other = bump_allocator<_U, _Alloc>;
    };

    bump_allocator()
    {
        impl = new bump_allocator_impl<_T, _Alloc>;
    }

    bump_allocator(bump_allocator const&) = default;
    bump_allocator(bump_allocator&&) = default;
    bump_allocator& operator=(bump_allocator const&) = default;

    value_type * allocate(size_t __n) const
    {
        return impl->allocate(__n);
    }

    void deallocate(value_type * __p, size_t __n) const
    {
        return impl->deallocate(__p, __n);
    }

private:
    refptr<bump_allocator_impl<_T, _Alloc>> impl;
};
#endif

template<typename T>
struct default_delete
{
    constexpr default_delete() = default;

    _always_inline void operator()(T* __ptr) const
    {
        delete __ptr;
    }
};

template<typename T>
struct default_delete<T[]>
{
    constexpr default_delete() = default;

    template<typename U>
    _always_inline void operator()(U __ptr) const
    {
        delete[] __ptr;
    }
};

template<typename _T, typename Tdeleter = default_delete<_T>>
class unique_ptr
{
public:
    using value_type = _T;
    using pointer = _T*;
    using const_pointer = _T const*;

    unique_ptr()
        : __ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& __rhs)
        : __ptr(__rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(_T* __value)
        : __ptr(__value)
    {
    }

    ~unique_ptr()
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
    }

    operator _T*()
    {
        return __ptr;
    }

    operator _T const*() const
    {
        return __ptr;
    }

    _T* operator->()
    {
        return __ptr;
    }

    _T const* operator->() const
    {
        return __ptr;
    }

    _T* get()
    {
        return __ptr;
    }

    _T const* get() const
    {
        return __ptr;
    }

    unique_ptr &operator=(_T* __rhs)
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
        __ptr = __rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

    _T* release()
    {
        _T* __p = __ptr;
        __ptr = nullptr;
        return __p;
    }

    bool reset(_T* p = pointer())
    {
        _T* __old = __ptr;
        __ptr = p;
        if (__old)
            ((Tdeleter()))(__old);

        return __ptr != nullptr;
    }

private:
    _T* __ptr;
};

template<typename _T, typename Tdeleter>
class unique_ptr<_T[], Tdeleter>
{
public:
    using value_type = _T;
    using pointer = _T*;
    using const_pointer = _T const*;

    unique_ptr()
        : __ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& rhs)
        : __ptr(rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(_T* __value)
        : __ptr(__value)
    {
    }

    ~unique_ptr()
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
    }

    operator _T*()
    {
        return __ptr;
    }

    operator _T const*() const
    {
        return __ptr;
    }

    _T& operator[](size_t i)
    {
        return __ptr[i];
    }

    _T const& operator[](size_t __i) const
    {
        return __ptr[__i];
    }

    _T* operator->()
    {
        return __ptr;
    }

    _T const* operator->() const
    {
        return __ptr;
    }

    _T* get()
    {
        return __ptr;
    }

    _T const* get() const
    {
        return __ptr;
    }

    unique_ptr &operator=(_T* __rhs)
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
        __ptr = __rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

    _T* release()
    {
        _T* __p = __ptr;
        __ptr = nullptr;
        return __p;
    }

    void reset(_T* __p = pointer())
    {
        _T* __old = __ptr;
        __ptr = __p;
        if (__old)
            ((Tdeleter()))(__old);
    }

private:
    _T* __ptr;
};

__END_NAMESPACE_STD
