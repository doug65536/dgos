#pragma once
#include "types.h"
#include "mutex.h"
#include "assert.h"

struct pool_base_t {
public:
    pool_base_t();
    ~pool_base_t();

    bool create(uint32_t item_size, uint32_t capacity);
    void *item(uint32_t index);
    void *alloc();
    void *calloc();
    void free(uint32_t index);

protected:
    using lock_type = ext::noirq_lock<ext::spinlock>;
    using scoped_lock = std::unique_lock<lock_type>;

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
        return (T*)pool_base_t::item(index);
    }

    template<typename... Args>
    T *alloc(Args&& ...args)
    {
        T *item = (T*)pool_base_t::alloc();
        if (likely(item))
            return new (item) T(std::forward<Args>(args)...);
        return nullptr;
    }

    template<typename... Args>
    T *calloc(Args&& ...args)
    {
        T *item = (T*)pool_base_t::calloc();
        return new (item) T(std::forward<Args>(args)...);
    }

    T *operator[](uint32_t index)
    {
        return (T*)pool_base_t::item(index);
    }

    template<typename U>
    void free(U* item)
    {
        size_t index = (T*)item - (T*)items;
        assert(index < item_capacity);
        item->~U();
        pool_base_t::free(index);
    }
};
