#pragma once

#include "types.h"
#include "mutex.h"
#include "basic_set.h"

//
// Contiguous allocator

struct contiguous_allocator_t {
    using lock_type = ext::irq_mutex;
    using scoped_lock = std::unique_lock<lock_type>;
public:
    using linaddr_t = uintptr_t;

    contiguous_allocator_t()
    {
    }

    void set_early_base(linaddr_t *addr);
    uintptr_t early_init(size_t size, char const *name);
    void init(linaddr_t addr, size_t size, char const *name);
    uintptr_t alloc_linear(size_t size);
    bool take_linear(linaddr_t addr, size_t size, bool require_free);
    void release_linear(uintptr_t addr, size_t size);
    void dump(char const *format, ...) const
    _printf_format(2, 3);
    void dumpv(char const *format, va_list ap) const
    _printf_format(2, 0);

    void dump_locked(scoped_lock& lock, char const *format, ...) const
    _printf_format(3, 4);

    void dump_lockedv(scoped_lock& lock, char const *format, va_list ap) const
    _printf_format(3, 0);

    bool validate() const;
    bool validate_locked(scoped_lock &lock) const;
    bool validation_failed(scoped_lock &lock) const;

    struct mmu_range_t {
        linaddr_t base;
        size_t size;
    };

    using tree_item_t = std::pair<uintptr_t, uintptr_t>;
    using tree_cmp_t = std::less<tree_item_t>;
    using tree_t = ext::fast_set<tree_item_t, tree_cmp_t>;

    template<typename F>
    void each_fw(F callback);

    template<typename F>
    void each_rv(F callback);

private:
    lock_type mutable free_addr_lock;
    tree_t free_addr_by_size;
    tree_t free_addr_by_addr;
    linaddr_t *linear_base_ptr = nullptr;
    bool ready = false;
    char const *name = nullptr;
};

template<typename F>
void contiguous_allocator_t::each_fw(F callback)
{
    static_assert(std::is_same<decltype((*(F*)nullptr)(mmu_range_t{})),
                  bool>::value,
                  "Callback must return boolean");

    scoped_lock lock(free_addr_lock);

    for (tree_t::const_iterator it = free_addr_by_addr.begin(),
         en = free_addr_by_addr.end(); it != en; ++it) {
        if (!callback(mmu_range_t{ it->first, it->second }))
            break;
    }
}

template<typename F>
void contiguous_allocator_t::each_rv(F callback)
{
    static_assert(std::is_same<decltype((*(F*)nullptr)(mmu_range_t{})),
                  bool>::value,
                  "Callback must return boolean");

    scoped_lock lock(free_addr_lock);

    for (tree_t::const_iterator it = free_addr_by_addr.rbegin(),
         en = free_addr_by_addr.rend(); it != en; ++it) {
        if (!callback(mmu_range_t{ it->first, it->second }))
            break;
    }
}
