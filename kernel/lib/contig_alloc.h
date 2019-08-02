#pragma once

#include "types.h"
#include "rbtree.h"
#include "mutex.h"

//
// Contiguous allocator

struct contiguous_allocator_t {
public:
    using linaddr_t = uintptr_t;

    void set_early_base(linaddr_t *addr);
    void early_init(size_t size, char const *name);
    void init(linaddr_t addr, size_t size, char const *name);
    uintptr_t alloc_linear(size_t size);
    bool take_linear(linaddr_t addr, size_t size, bool require_free);
    void release_linear(uintptr_t addr, size_t size);
    void dump(char const *format, ...);

    struct mmu_range_t {
        linaddr_t base;
        size_t size;
    };

    template<typename F>
    void each_fw(F callback);

    template<typename F>
    void each_rv(F callback);
private:
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type free_addr_lock;
    typedef rbtree_t<> tree_t;
    tree_t free_addr_by_size;
    tree_t free_addr_by_addr;
    linaddr_t *linear_base_ptr;
    char const *name;
};

template<typename F>
void contiguous_allocator_t::each_fw(F callback)
{
    static_assert(std::is_same<decltype((*(F*)nullptr)(mmu_range_t{})),
                  bool>::value,
                  "Callback must return boolean");

    scoped_lock lock(free_addr_lock);

    for (tree_t::iter_t it = free_addr_by_addr.last(0);
         it; it = free_addr_by_addr.next(it)) {
        tree_t::kvp_t const& item = free_addr_by_addr.item(it);

        if (!callback(mmu_range_t{ item.key, item.val }))
            break;

        it = free_addr_by_addr.next(it);
    }
}

template<typename F>
void contiguous_allocator_t::each_rv(F callback)
{
    static_assert(std::is_same<decltype((*(F*)nullptr)(mmu_range_t{})),
                  bool>::value,
                  "Callback must return boolean");

    scoped_lock lock(free_addr_lock);

    for (tree_t::iter_t it = free_addr_by_addr.last(0);
         it; it = free_addr_by_addr.prev(it)) {
        tree_t::kvp_t const& item = free_addr_by_addr.item(it);

        if (!callback(mmu_range_t{ item.key, item.val }))
            break;
    }
}
