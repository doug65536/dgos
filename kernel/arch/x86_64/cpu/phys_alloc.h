#pragma once
#include "types.h"
#include "mutex.h"
#include "mmu.h"
#include "printk.h"

using physaddr_t = uintptr_t;
using linaddr_t = uintptr_t;

class mmu_phys_allocator_t {
    typedef uint32_t entry_t;
    using lock_type = ext::noirq_lock<ext::spinlock>;
    using scoped_lock = std::unique_lock<lock_type>;
public:
    static size_t size_from_highest_page(physaddr_t page_index);

    constexpr mmu_phys_allocator_t() = default;

    void init(void *addr, physaddr_t begin_, size_t highest_usable,
              uint8_t log2_pagesz_ = PAGE_SIZE_BIT);

    void add_free_space(physaddr_t base, size_t size);

    // Unreliably peek and see if there might be a free page, outside lock
    operator bool() const
    {
        return next_free != entry_t(-1);
    }

    physaddr_t alloc_one();

    // Take multiple pages and receive each physical address in callback
    // Returns false with no memory allocated on failure
    template<typename F>
    bool _always_inline alloc_multiple(size_t size, F callback);

    void release_one(physaddr_t addr);

    void addref(physaddr_t addr);

    void validate();

    void adjref_virtual_range(linaddr_t start, size_t len, int adj);

    class free_batch_t {
    public:
        bool empty() const noexcept
        {
            return count == 0;
        }

        physaddr_t pop() noexcept
        {
            return pages[--count];
        }

        void free(physaddr_t addr) noexcept
        {
            // Can't free demand page entry
            if (unlikely(!addr || addr == PTE_ADDR))
                return;

            if (unlikely(count == countof(pages)))
                flush();

            pages[count++] = addr;
        }

        void flush() noexcept
        {
            scoped_lock lock(owner.alloc_lock);
            // Heuristic that weakly attempts to free pages so they will be
            // linked back into the free chain in an order that causes
            // subsequent allocations to return blocks in ascending order
            if (count < 2 || pages[0] > pages[1]) {
                // Order doesn't matter or first is higher than second
                for (size_t i = 0; i < count; ++i)
                    owner.release_one_locked(pages[i], lock);
            } else {
                // Second is higher than first, release in reverse order
                for (size_t i = count; i > 0; --i)
                    owner.release_one_locked(pages[i-1], lock);
            }
            count = 0;
        }

        explicit free_batch_t(mmu_phys_allocator_t& owner) noexcept
            : owner(owner)
            , count(0)
        {
        }

        ~free_batch_t()
        {
            if (count)
                flush();
        }

        free_batch_t(free_batch_t const&) = default;

    private:
        physaddr_t pages[16];
        mmu_phys_allocator_t &owner;
        unsigned count;
    };

    // Not locked but it is approximate because it is stale information anyway
    _always_inline uint64_t get_free_page_count() const noexcept
    {
        return free_page_count;
    }

    _always_inline uint64_t get_phys_mem_size() const noexcept
    {
        return highest_usable;
    }

private:
    _always_inline size_t index_from_addr(physaddr_t addr) const noexcept
    {
        return (addr - begin) >> log2_pagesz;
    }

    _always_inline physaddr_t addr_from_index(size_t index) const noexcept
    {
        return (index << log2_pagesz) + begin;
    }

    _always_inline bool release_one_locked(
            physaddr_t addr, scoped_lock&) noexcept
    {
        size_t index = index_from_addr(addr);
        if (unlikely(!assert(index < highest_usable)))
            return false;
        assert(entries[index] & used_mask);
        if (entries[index] == (1 | used_mask)) {
            // Free the page
            entries[index] = next_free;
            next_free = index;
            ++free_page_count;
#if DEBUG_PHYS_ALLOC
            printdbg("Freed page @ %#zx\n", addr);
#endif
            return true;
        } else {
            // Reduce reference count
            --entries[index];
#if DEBUG_PHYS_ALLOC
            printdbg("Reduced reference count @ %#zx to %u\n",
                     addr, entries[index]);
#endif
        }

        return false;
    }

    static constexpr entry_t used_mask =
            (entry_t(1) << (sizeof(entry_t) * 8 - 1));

    entry_t *entries = nullptr;
    physaddr_t begin = 0;
    entry_t next_free = 0;
    entry_t free_page_count = 0;
    lock_type alloc_lock;
    uint8_t log2_pagesz = 0;
    size_t highest_usable = 0;
};


template<typename F>
bool mmu_phys_allocator_t::alloc_multiple(size_t size, F callback)
{
    // Assert that the size is a multiple of the page size
    assert(!(size & ((size_t(1) << log2_pagesz)-1)));

    size_t count = size >> log2_pagesz;

    if (unlikely(!count))
        return true;

#if DEBUG_PHYS_ALLOC
    printdbg("Allocating %zu pages, low=%d\n", count, low);
#endif

    scoped_lock lock(alloc_lock);

    entry_t first;

    for (;;) {
        first = next_free;
        entry_t new_next = first;
        assert(!(new_next & used_mask));

        size_t i;
        for (i = 0; i < count && new_next != entry_t(-1); ++i) {
            assert(new_next < highest_usable);
            new_next = entries[new_next];
            assert(new_next == entry_t(-1) || !(new_next & used_mask));
        }

        // If we found enough pages, commit the change
        if (i == count) {
            // Have to write these out because we are leaving the lock
            next_free = new_next;
            free_page_count -= count;

            if (free_page_count < 256)
                printdbg("WARNING: Under 1MB free! Continuing...\n");

            break;
        } else {
            //assert(!"Out of memory!");
            printdbg("Out of memory! Continuing...\n");
            return false;
        }
    }

    lock.unlock();

    size_t range_count = size >> log2_pagesz;
    size_t used_count = 0;

    for (size_t i = 0; i < range_count && used_count < count; ++i) {
        entry_t next = entries[first];
        assert(!(next & used_mask));

        physaddr_t paddr = addr_from_index(first);

#if DEBUG_PHYS_ALLOC
    printdbg("...providing page to callback, addr=%p\n", (void*)paddr);
#endif

        // Call callable with physical address
        if (callback(i, log2_pagesz, paddr)) {
            // Set reference count to 1
            entries[first] = 1 | used_mask;
            ++used_count;
        } else {
#if DEBUG_PHYS_ALLOC
            printdbg("......callback didn't need it\n");
#endif
            // Revert back
            next = first;
        }

        // Follow chain to next free
        first = next;
    }

    if (used_count != count) {
        // Splice the remaining pages onto the free chain
        lock.lock();

        free_page_count += count - used_count;

        next_free = first;
    }

    return true;
}

HIDDEN extern mmu_phys_allocator_t phys_allocator;
