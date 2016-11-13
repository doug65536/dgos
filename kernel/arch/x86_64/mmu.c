#include "mmu.h"
#include "msr.h"
#include "printk.h"

#define PAGE_SIZE_BITS  12
#define PAGE_SIZE       (1<<PAGE_SIZE_BITS)

physmem_range_t *phys_mem_map;

void mmu_init(void)
{
    uint64_t usable_pages = 0;
    uint64_t highest_usable = 0;
    for (physmem_range_t *mem = phys_mem_map;
         mem->valid; ++mem) {
        printk("Memory: addr=%lx size=%lx type=%x\n",
               mem->base, mem->size, mem->type);

        if (mem->base >= 0x100000) {
            uint64_t rounded_size = mem->size;
            uint64_t rounded_base = (mem->base + (PAGE_SIZE-1)) & (uint64_t)-PAGE_SIZE;

            rounded_size -= rounded_base - mem->base;
            rounded_size &= (uint64_t)-PAGE_SIZE;

            uint64_t rounded_end = rounded_base + rounded_size;

            if (highest_usable < rounded_end)
                highest_usable = rounded_end;

            if (PHYSMEM_TYPE_NORMAL)
                usable_pages += rounded_size >> 12;
        }
    }

    // Exclude pages below 1MB line from physical page allocation map
    highest_usable -= 0x100000;

    // Compute number of slots needed
    highest_usable >>= PAGE_SIZE_BITS;
    printk("Usable pages = %lu (%luMB) range_pages=%ld\n", usable_pages,
           usable_pages >> (20 - PAGE_SIZE_BITS), highest_usable);

    /// Physical page allocation map
    /// Consists of array of entries, one per physical page
    /// Each slot contains the index of the next slot,
    /// which forms a singly-linked list of pages.
    /// Free pages are linked into a chain, allowing for O(1)
    /// performance allocation and freeing of physical pages.
    ///
    /// 2^32 pages is 2^(32+12-40) bytes = 16TB
    ///

}

void mmu_set_fsgsbase(void *fs_base, void *gs_base)
{
    msr_set(MSR_FSBASE, (uint64_t)fs_base);
    msr_set(MSR_GSBASE, (uint64_t)gs_base);
}
