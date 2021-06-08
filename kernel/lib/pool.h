#pragma once
#include "types.h"
#include "mutex.h"
#include "assert.h"
#include "heap.h"

struct KERNEL_API pool_base_t {
public:
    pool_base_t();
    ~pool_base_t();

    bool create(uint32_t item_size, uint32_t capacity);
    inline void *item(uint32_t index);
    void *alloc();
    void *calloc();
    void free(uint32_t index);

protected:
    using lock_type = ext::noirq_lock<ext::spinlock>;
    using scoped_lock = ext::unique_lock<lock_type>;

    char *items;
    uint32_t item_size;
    uint32_t item_capacity;
    uint32_t item_count;
    uint32_t first_free;
    lock_type pool_lock;
};

template<typename T>
class pool_t : public pool_base_t {
public:
    pool_t() = default;
    ~pool_t() = default;
    pool_t(pool_t const&) = default;

    bool create(uint32_t capacity)
    {
        return pool_base_t::create(sizeof(T), capacity);
    }

    T *item(uint32_t index)
    {
        return reinterpret_cast<T*>(pool_base_t::item(index));
    }

    template<typename... Args>
    T *alloc(Args&& ...args)
    {
        void *mem = pool_base_t::alloc();

#if HEAP_DEBUG
        memset(mem, 0xf0, sizeof(T));
#endif

        if (likely(mem))
            return new (mem) T(ext::forward<Args>(args)...);

        return nullptr;
    }

    template<typename... Args>
    T *calloc(Args&& ...args)
    {
        T *item = (T*)pool_base_t::calloc();
        return new (item) T(ext::forward<Args>(args)...);
    }

    T *operator[](uint32_t index)
    {
        return (T*)pool_base_t::item(index);
    }

    void free(T* item)
    {
        if (item) {
            size_t index = item - reinterpret_cast<T*>(items);
            assert(index < item_capacity);
            item->~T();

#if HEAP_DEBUG
            memset(reinterpret_cast<void*>(item), 0xfe, sizeof(T));
#endif

            pool_base_t::free(index);
        }
    }

//    template<typename U>
//    void free(U* item)
//    {
//        size_t index = static_cast<T*>(item) - static_cast<T*>(items);
//        assert(index < item_capacity);
//        item->~U();
//        pool_base_t::free(index);
//    }
};
