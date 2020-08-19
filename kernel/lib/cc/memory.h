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
#include "type_traits.h"

#include "printk.h"

#include "heap.h"

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

    allocator() = default;
    allocator(allocator const& __other) = default;

    template<typename _U>
    allocator(allocator<_U> const& __other)
    {
    }

    value_type *allocate(size_t __n) const
    {
        return reinterpret_cast<value_type*>(malloc(__n * sizeof(value_type)));
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

    allocator()
    {
    }

    template<typename _U>
    allocator(allocator<_U> const& rhs)
    {
    }
};
__END_NAMESPACE_STD

#ifdef __DGOS_KERNEL__

__BEGIN_NAMESPACE_EXT
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
        using other = page_allocator<_U>;
    };

    value_type *allocate(size_t __n) const
    {
        return reinterpret_cast<value_type*>(
                    allocate_impl(__n * sizeof(value_type)));
    }

    void deallocate(value_type * p, size_t n) const
    {
        deallocate_impl(p, n * sizeof(value_type));
    }
};

template<>
struct page_allocator<void>
{
    template<typename _U>
    struct rebind
    {
        using other = page_allocator<_U>;
    };
};

template<typename T, typename Alloc = std::allocator<char>,
         size_t chunk_size = PAGESIZE << 6>
class bump_allocator_impl
    : public refcounted<bump_allocator_impl<T>>
{
private:
    using Allocator = typename Alloc::template rebind<char>::other;
    struct freetag_t {};
    struct emplace_tag_t {};

    /// +-----------+
    /// | items...  |
    /// | items...  |
    /// +-----------+
    /// | next page |
    /// +-----------+

    union item_t
    {
        // Either a data item...
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;

        // Or a free chain pointer
        item_t *next_free;
    };

    // Calculate how many items fit, leaving room for page chain pointer
    static constexpr size_t page_capacity =
            (chunk_size - sizeof(void*)) / sizeof(T);

public:
    using value_type = T;

    bump_allocator_impl()
    {
        current_page = nullptr;
        bump_index = page_capacity;
        first_free = nullptr;
        last_free = nullptr;
    }

    void destroy() override final
    {
        reinterpret_cast<T*>(this)->~T();
        alloc.deallocate(reinterpret_cast<char*>(this),
                         sizeof(bump_allocator_impl));
    }

    T *allocate(size_t __n)
    {
        assert(__n < page_capacity);
        assert(__n == 1);

        size_t remain = page_capacity - bump_index;

        // Reuse block if possible
        if (likely(first_free))
        {
            T *result = reinterpret_cast<T*>(first_free->storage.data);
//            printdbg("bump allocator reusing freed block at %#zx\n",
//                     uintptr_t(result));

            // Advance first_free to next item
            first_free = first_free->next_free;

            // Null out last_free if first_free became null
            last_free = first_free ? last_free : nullptr;

            return result;
        }

        // Fastpath allocation when it would fit within capacity
        if (likely(remain >= __n)) {
//            printdbg("bump allocator allocated"
//                     " and bumped from [%#zx] at %#zx\n", bump_index,
//                     uintptr_t(current_page[bump_index].storage.data));

            // Most likely case, we have room in current page
            T *result = reinterpret_cast<T*>(
                        current_page[bump_index++].storage.data);

            return result;
        }

        // Page full or first allocation ever

        printdbg("bump allocator adding a new page\n");

        // Link new page back to this page
        item_t *prev_page = current_page;

        // Allocate a new page
        current_page = reinterpret_cast<item_t*>(alloc.allocate(chunk_size));

        // Pointer sized bytes at the end of the page
        // are reserved for previous page chain
        char *prev_page_ptr = reinterpret_cast<char*>(current_page) +
                chunk_size - sizeof(prev_page);
        memcpy(prev_page_ptr, &prev_page, sizeof(prev_page));

        // Reset index to start of freshly allocated page
        bump_index = 0;

//        printdbg("bump allocator allocated"
//                 " and bumped from [%#zx] at %#zx\n", bump_index,
//                 uintptr_t(current_page[bump_index].storage.data));

        return reinterpret_cast<T*>(
                    current_page[bump_index++].storage.data);
    }

    void deallocate(T *__p, size_t __n)
    {
        assert(__n == 1);
//        printdbg("bump allocator item freed at %#zx\n",
//                 uintptr_t(__p));

        // Either write next pointer into last block, or,
        // write that into first_free instead
        item_t **adj = last_free ? &last_free->next_free : &first_free;
        *adj = reinterpret_cast<item_t*>(__p);

        // Newly deallocated block is reused last
        // after all previously freed blocks
        // This should reduce temporal locality but increase spacial locality
        last_free = reinterpret_cast<item_t*>(__p);

        // Null out next pointer in free block chain
        *reinterpret_cast<void**>(__p) = nullptr;
    }

private:
    item_t *current_page;
    size_t bump_index;

    // Rebind incoming allocator to allocate item_t objects
    Allocator alloc;

    item_t *first_free;
    item_t *last_free;
};

// The stateful allocator object is just a refcounted pointer to
// an underlying (and shared by copies) bump_allocator_impl
template<typename _T, typename _Alloc>
class bump_allocator
{
    using _Allocator = typename _Alloc::template rebind<char>::other;
public:
    using value_type = _T;

    static_assert(sizeof(_T) >= sizeof(void*),
                  "Type is too small to bump allocate");

    template<typename _U>
    struct rebind
    {
        using other = bump_allocator<_U, _Alloc>;
    };

    bump_allocator()
    {
    }

    bump_allocator(bump_allocator const&) = default;
    bump_allocator(bump_allocator&&) noexcept = default;
    bump_allocator& operator=(bump_allocator const&) = default;

    template<typename _U>
    bump_allocator(bump_allocator<_U, _Alloc> const& rhs)
        : impl(rhs.impl.detach())
    {
    }

    void create()
    {
        if (unlikely(!impl)) {
            void *mem = _Allocator().allocate(
                        sizeof(bump_allocator_impl<_T, _Alloc>));
            impl = new (mem) bump_allocator_impl<_T, _Alloc>();
        }
    }

    value_type *allocate(size_t __n)
    {
        if (unlikely(!impl))
            create();

        value_type *mem = impl->allocate(__n);

#if HEAP_DEBUG
        if (mem)
            memset(reinterpret_cast<void*>(mem),
                   0xf0, sizeof(value_type) * __n);
#endif

        return mem;
    }

    void deallocate(value_type * __p, size_t __n)
    {
#if HEAP_DEBUG
        memset(reinterpret_cast<void*>(__p),
               0xfe, sizeof(value_type) * __n);
#endif
        return impl->deallocate(__p, __n);
    }

private:
    refptr<bump_allocator_impl<_T, _Alloc>> impl;
};

template<typename _Alloc>
class bump_allocator<void, _Alloc>
{
public:
    template<typename _U>
    struct rebind
    {
        using other = bump_allocator<_U, _Alloc>;
    };

    using _Allocator = typename _Alloc::template rebind<char>::other;

    bump_allocator(bump_allocator const& rhs) = default;

    ~bump_allocator() = default;

    template<typename _U>
    bump_allocator(bump_allocator<_U, _Alloc>&& rhs)
        : impl(rhs.detach())
    {
    }

    bump_allocator *detach()
    {
        return impl.detach();
    }

private:
    refptr<bump_allocator_impl<void, _Alloc>> impl;
};
__END_NAMESPACE_EXT

#endif

__BEGIN_NAMESPACE_STD
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

    unique_ptr() noexcept
        : __ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& __rhs) noexcept
        : __ptr(__rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(_T* __value) noexcept
        : __ptr(__value)
    {
    }

    ~unique_ptr()
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
    }

    operator _T*() noexcept
    {
        return __ptr;
    }

    operator _T const*() const noexcept
    {
        return __ptr;
    }

    _T* operator->() noexcept
    {
        return __ptr;
    }

    _T const* operator->() const noexcept
    {
        return __ptr;
    }

    _T* get() noexcept
    {
        return __ptr;
    }

    _T const* get() const noexcept
    {
        return __ptr;
    }

    unique_ptr &operator=(_T* __rhs) noexcept
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

    _T* release() noexcept
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

    unique_ptr() noexcept
        : __ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& rhs) noexcept
        : __ptr(rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(_T* __value) noexcept
        : __ptr(__value)
    {
    }

    ~unique_ptr()
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
    }

    operator _T*() noexcept
    {
        return __ptr;
    }

    operator _T const*() const noexcept
    {
        return __ptr;
    }

    _T& operator[](size_t i) noexcept
    {
        return __ptr[i];
    }

    _T const& operator[](size_t __i) const noexcept
    {
        return __ptr[__i];
    }

    _T* operator->() noexcept
    {
        return __ptr;
    }

    _T const* operator->() const noexcept
    {
        return __ptr;
    }

    _T* get() noexcept
    {
        return __ptr;
    }

    _T const* get() const noexcept
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

    _T* release() noexcept
    {
        _T* __p = __ptr;
        __ptr = nullptr;
        return __p;
    }

    bool reset(_T* __p = pointer())
    {
        _T* __old = __ptr;
        __ptr = __p;

        if (__old)
            ((Tdeleter()))(__old);

        return __ptr != nullptr;
    }

private:
    _T* __ptr;
};

__END_NAMESPACE_STD

#include "cpu/control_regs_constants.h"
#include "thread.h"

__BEGIN_NAMESPACE_EXT
template<typename T>
class per_cpu_t
{
    void init()
    {
        cpu_count = thread_get_cpu_count();
        for (auto it = storage, en = storage + cpu_count; it != en; ++it)
            new (it) T();
    }

    template<typename ...Args>
    void init(Args&& ...args)
    {
        size_t cpu_count = thread_get_cpu_count();
        for (auto it = storage, en = storage + cpu_count; it != en; ++it)
            new (it) T(std::forward<Args>(args)...);
    }

    ~per_cpu_t()
    {
        for (auto it = storage, en = storage + cpu_count; it != en; ++it)
            ((T*)(it))->~T();
    }

    typename std::aligned_storage<sizeof(T)>::type storage[MAX_CPUS];
    size_t cpu_count = 0;
};
__END_NAMESPACE_EXT
