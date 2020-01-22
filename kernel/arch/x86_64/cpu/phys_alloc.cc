#include "phys_alloc.h"
#include "algorithm.h"

#define DEBUG_PHYS_ALLOC        0

mmu_phys_allocator_t phys_allocator;

EXPORT size_t mmu_phys_allocator_t::size_from_highest_page(
        physaddr_t page_index)
{
    return page_index * sizeof(entry_t);
}

EXPORT void mmu_phys_allocator_t::init(
        void *addr, physaddr_t begin_,
        size_t highest_usable_, uint8_t log2_pagesz_)
{
    entries = (entry_t*)addr;
    begin = begin_;
    log2_pagesz = log2_pagesz_;
    highest_usable = highest_usable_;
    next_free = entry_t(-1);

    std::fill_n(entries, highest_usable_, next_free);
}

EXPORT void mmu_phys_allocator_t::add_free_space(physaddr_t base, size_t size)
{
#if DEBUG_PHYS_ALLOC
    printdbg("Adding free space, base=%#zx, length=%#zx\n",
             base, size);
#endif

    scoped_lock lock(alloc_lock);
    physaddr_t free_end = base + size;
    size_t pagesz = size_t(1) << log2_pagesz;
    entry_t index = index_from_addr(free_end) - 1;
    while (size != 0) {
        assert(index < highest_usable);
        assert(entries[index] == entry_t(-1));
        entries[index] = next_free;
        next_free = index;
        --index;
        size -= pagesz;
        ++free_page_count;
    }
}

EXPORT physaddr_t mmu_phys_allocator_t::alloc_one()
{
    scoped_lock lock_(alloc_lock);

    size_t index = next_free;

    if (unlikely(index == entry_t(-1))) {
        printdbg("Out of memory! Continuing...\n");
        return 0;
    }

    assert(index < highest_usable);

    // Follow chain to next free
    entry_t new_next = entries[index];

    // Make sure the page we just got from free chain isn't marked used
    assert(new_next == entry_t(-1) || !(new_next & used_mask));

    // Mark used and initialize refcount to 1
    entries[index] = used_mask | 1;

    // Advance first free to next free
    next_free = new_next;

    lock_.unlock();

    physaddr_t addr = addr_from_index(index);

#if DEBUG_PHYS_ALLOC
    printdbg("Allocated page, low=%d, page=%p\n", low, (void*)addr);
#endif

    return addr;
}

EXPORT void mmu_phys_allocator_t::release_one(physaddr_t addr)
{
    scoped_lock lock(alloc_lock);
    release_one_locked(addr, lock);
}

EXPORT void mmu_phys_allocator_t::addref(physaddr_t addr)
{
    entry_t index = index_from_addr(addr);
    assert(index < highest_usable);
    scoped_lock lock_(alloc_lock);
    assert(entries[index] & used_mask);
    ++entries[index];
}

EXPORT void mmu_phys_allocator_t::validate()
{
    size_t free_count = 0;
    for (entry_t ent = next_free; ent != entry_t(-1); ent = entries[ent]) {
        ++free_count;
        uintptr_t addr = addr_from_index(ent);
        printdbg("Free page at %#zx\n", addr);
    }
    assert(free_page_count == free_count);
}

EXPORT void mmu_phys_allocator_t::adjref_virtual_range(
        linaddr_t start, size_t len, int adj)
{
    unsigned misalignment = start & PAGE_SCALE;
    start -= misalignment;
    len += misalignment;
    len = round_up(len);

    pte_t *ptes[4];
    ptes_from_addr(ptes, start);

    size_t count = len >> log2_pagesz;

    free_batch_t free_batch(phys_allocator);

    scoped_lock lock_(alloc_lock);

    if (adj > 0) {
        for (size_t i = 0; i < count; ++i) {
            physaddr_t addr = *ptes[3] & PTE_ADDR;

            if (addr && addr != PTE_ADDR) {
                entry_t index = index_from_addr(addr);
                assert(index < highest_usable);
                assert(entries[index] & used_mask);
                ++entries[index];
            }

            ++ptes[3];
        }
    } else if (adj < 0) {
        for (size_t i = 0; i < count; ++i) {
            physaddr_t addr = *ptes[3] & PTE_ADDR;

            if (pte_is_sysmem(*ptes[3])) {
                entry_t index = index_from_addr(addr);
                assert(entries[index] & used_mask);
                if (--entries[index] == 0)
                    free_batch.free(addr_from_index(index));
            }

            ++ptes[3];
        }
    }
}
