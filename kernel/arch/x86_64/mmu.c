#include "mmu.h"
#include "msr.h"
#include "printk.h"
#include "time.h"

// Intel manual, page 2786

// The entries of the 4 levels of page tables are named:
//  PML4E (maps 512 512GB regions)
//  PDPTE (maps 512 1GB regions)
//  PDE   (maps 512 2MB regions)
//  PTE   (maps 512 4KB regions)

// 4KB pages
#define PAGE_SIZE_BIT       12
#define PAGE_SIZE           (1U << PAGE_SIZE_BIT)
#define PAGE_MASK           (PAGE_SIZE - 1U)

// Page table entries
#define PTE_PRESENT_BIT     0
#define PTE_WRITABLE_BIT    1
#define PTE_USER_BIT        2
#define PTE_PWT_BIT         3
#define PTE_PCD_BIT         4
#define PTE_ACCESSED_BIT    5
#define PTE_DIRTY_BIT       6
#define PTE_PAGESIZE_BIT    7
#define PTE_GLOBAL_BIT      8
#define PTE_PAT_BIT         12  // only PDPTE and PDE
#define PTE_ADDR_BIT        12
#define PTE_PK_BIT          59
#define PTE_NX_BIT          63

// Always ignored areas 11:9 58:52
#define PTE_AVAIL1_BIT      9
#define PTE_AVAIL2_BIT      51

// Size of multi-bit fields
#define PTE_PK_BITS         4
#define PTE_ADDR_BITS       28
#define PTE_AVAIL1_BITS     3
#define PTE_AVAIL2_BITS     7

// Size of physical address including low bits
#define PTE_FULL_ADDR_BITS  (PTE_ADDR_BITS+PAGE_SIZE_BIT)

// Bitmask for multi-bit field values
// Aligned to bit 0
#define PTE_PK_MASK         ((1ULL << PTE_PK_BITS) - 1U)
#define PTE_ADDR_MASK       ((1ULL << PTE_ADDR_BITS) - 1U)
#define PTE_AVAIL1_MASK     ((1ULL << PTE_AVAIL1_BITS) - 1U)
#define PTE_AVAIL2_MASK     ((1ULL << PTE_AVAIL2_BITS) - 1U)
#define PTE_FULL_ADDR_MASK  ((1ULL << PTE_FULL_ADDR_BITS) - 1U)

// Values of bits
#define PTE_PRESENT         (1ULL << PTE_PRESENT_BIT)
#define PTE_WRITABLE        (1ULL << PTE_WRITABLE_BIT)
#define PTE_USER            (1ULL << PTE_USER_BIT)
#define PTE_PWT             (1ULL << PTE_PWT_BIT)
#define PTE_PCD             (1ULL << PTE_PCD_BIT)
#define PTE_ACCESSED        (1ULL << PTE_ACCESSED_BIT)
#define PTE_DIRTY           (1ULL << PTE_DIRTY_BIT)
#define PTE_PAGESIZE        (1ULL << PTE_PAGESIZE_BIT)
#define PTE_GLOBAL          (1ULL << PTE_GLOBAL_BIT)
#define PTE_PAT             (1ULL << PTE_PAT_BIT)
#define PTE_NX              (1ULL << PTE_NX_BIT)

// Multi-bit field masks, in place
#define PTE_ADDR            (PTE_ADDR_MASK << PTE_ADDR_BIT)
#define PTE_PK              (PTE_PK_MASK << PTE_PK_BIT)
#define PTE_AVAIL1          (PTE_AVAIL1_MASK << PTE_AVAIL1_BIT)
#define PTE_AVAIL2          (PTE_AVAIL2_MASK << PTE_AVAIL2_BIT)

// Get field
#define GF(pte, field) \
    (((pte) >> PTE_##field##_BIT) & PTE_##field##_MASK)

// Set field
#define SF(pte, field, value) \
    ((((pte) >> PTE_##field##_BIT) & ~PTE_##field##_MASK) | \
    (((value) & PTE_##field##_MASK) << PTE_##field##_BIT);

// Write field
#define WF(pte, field, value) ((pte) = SF((pte), (field), (value)))

/// Mapping the page tables
/// -----------------------
///
/// To allow O(1) lookup of the page tables corresponding to
/// a linear address, the top of linear address space is
/// reserved for mapping the page tables.
///
/// The kernel has a map for the range
/// 0x400000000000 to 0x800000000000
/// This range is 70,368,744,177,664 bytes
///
/// One page maps 2MB: 33554432 pages (0x2000000)
/// plus
/// One page maps 1GB: 65536 pages (0x10000)
/// plus
/// One page maps 512GB: 128 pages (0x00080)
///
/// 33554432 + 65536 + 128 = 33620096 pages
///  (0x2000000 + 0x10000 + 0x80)
///
/// 33620096 pages = 137,707,913,216 bytes
///

// Page table entries don't have a structure, they
// are a bunch of bitfields. Use uint64_t and the
// constants above
typedef uint64_t pte_t;

// Root directory entry 254 and 255 are reserved
// for mapping page tables. 255 maps the kernel
// page tables, 254 maps the process page tables.
// This allows processes mappings to trivially
// share the kernel mapping by copying entry 255
// of the root directory entry.
//
// The user page tables are mapped at
//  0x7f0000000000
// The kernel page tables are mapped at
//  0x7f8000000000

// Linear addresses
#define PT3_KBASEADDR   (0x7F8000000000L)
#define PT3_UBASEADDR   (0x7F0000000000L)

// The number of pte_t entries at each level
#define PT3_ENTRIES     (0x080000000000L)
#define PT2_ENTRIES     (0x000400000000L)
#define PT1_ENTRIES     (0x000002000000L)
#define PT0_ENTRIES     (0x000000010000L)
#define PTR_ENTRIES     (0x000000000080L)

// The array index of the start of each level
#define PT3_BASE        (0)
#define PT2_BASE        (PT3_BASE + PT3_ENTRIES)
#define PT1_BASE        (PT2_BASE + PT2_ENTRIES)
#define PT0_BASE        (PT1_BASE + PT1_ENTRIES)

// Addresses of start of page tables for each level
#define PT3_PTR(base)  ((pte_t*)(base))
#define PT2_PTR(base)  (PT3_PTR(base) + PT3_ENTRIES)
#define PT1_PTR(base)  (PT2_PTR(base) + PT2_ENTRIES)
#define PT0_PTR(base)  (PT1_PTR(base) + PT1_ENTRIES)
#define PTD_PTR(base)  (PT0_PTR(base) + PT0_ENTRIES)

static pte_t * const ptn_base[4] = {
    PT3_PTR(0),
    PT3_PTR(1),
    PT3_PTR(2),
    PT3_PTR(3)
};

physmem_range_t *phys_mem_map;
size_t phys_mem_map_count;

static void mmu_mem_map_swap(physmem_range_t *a, physmem_range_t *b)
{
    physmem_range_t temp = *a;
    *a = *b;
    *b = temp;
}

// Align bases to page boundaries
// Align sizes to multiples of page size
// Sort by base address
// Fix overlaps
static void mmu_fixup_mem_map(physmem_range_t *list)
{
    int did_something;

    // Might need multiple passes to fully fixup the list
    do {
        did_something = 0;

        // Simple sort algorithm for small list.
        // Bubble sort is actually fast for nearly-sorted
        // or already-sorted list. The list is probably
        // already sorted
        for (size_t i = 1; i < phys_mem_map_count; ++i) {
            // Slide it back to where it should be
            for (size_t j = i; j > 0; --j) {
                if (list[j - 1].base < list[j].base)
                    break;

                mmu_mem_map_swap(list + j - 1, list + j);
                did_something = 1;
            }
        }

        if (did_something) {
            printk("Memory map order fixed up\n");
            continue;
        }

        // Discard entries marked for removal from the end
        while (phys_mem_map_count > 0 &&
               list[phys_mem_map_count-1].base == ~0UL &&
               list[phys_mem_map_count-1].size == ~0UL) {
            --phys_mem_map_count;
            did_something = 1;
        }

        if (did_something) {
            printk("Memory map entries were eliminated\n");
            continue;
        }

        // Fixup overlaps
        for (size_t i = 1; i < phys_mem_map_count; ++i) {
            if (list[i-1].base + list[i-1].size > list[i].base) {
                // Overlap

                // If both same type, truncate first one
                // and make second one cover combined range
                if (list[i-1].type == list[i].type) {
                    // Handle every possible overlap case

                    // Compute bounds of prev
                    uint64_t prev_st = list[i-1].base;
                    uint64_t prev_en = prev_st + list[i-1].size;

                    // Compute bounds of this
                    uint64_t this_st = list[i].base;
                    uint64_t this_en = this_st + list[i].size;

                    // Start at lowest bound, end at highest bound
                    uint64_t st = prev_st < this_st ? prev_st : this_st;
                    uint64_t en = prev_en > this_en ? prev_en : this_en;

                    // Make prev get dragged to the end of the list next pass
                    list[i-1].base = ~0UL;
                    list[i-1].size = ~0UL;

                    // Keep the second one because it could get coalesced
                    // with the next one
                    list[i].base = st;
                    list[i].size = en - st;

                    // Need another pass
                    did_something = 1;
                } else if (list[i-1].type == PHYSMEM_TYPE_NORMAL) {
                    // Truncate length of previous entry to avoid
                    // overlapping current

                    uint64_t new_size = list[i].base - list[i-1].base;

                    if (new_size > 0) {
                        list[i-1].size = new_size;
                    } else {
                        // Entry got eliminated
                        list[i-1].base = ~0UL;
                        list[i-1].size = ~0UL;
                    }

                    // Need another pass
                    did_something = 1;
                } else if (list[i].type == PHYSMEM_TYPE_NORMAL) {
                    // Move base forward to avoid previous entry

                    uint64_t prev_en = list[i-1].base + list[i-1].size;
                    uint64_t this_en = list[i].base + list[i].size;

                    if (this_en > prev_en) {
                        list[i].base = prev_en;
                        list[i].size = this_en - prev_en;
                    } else {
                        // Entry got eliminated
                        list[i].base = ~0UL;
                        list[i].size = ~0UL;
                    }

                    // Need another pass
                    did_something = 1;
                }
            }
        }

        if (did_something) {
            printk("Memory map overlap fixed up\n");
            continue;
        }

        // Fixup page alignment
        for (size_t i = 0; i < phys_mem_map_count; ++i) {
            if (list[i].type == PHYSMEM_TYPE_NORMAL) {
                uint64_t st = list[i].base;
                uint64_t en = list[i].base + list[i].size;

                // Align start to a page boundary
                st += PAGE_MASK;
                st -= st & PAGE_MASK;

                // Align end to a page boundary
                en -= en & PAGE_MASK;

                if (st != list[i].base ||
                        en != list[i].base + list[i].size) {
                    if (en > st) {
                        list[i].base = st;
                        list[i].size = en - st;
                    } else {
                        list[i].base = ~0UL;
                        list[i].size = ~0UL;
                    }
                    did_something = 1;
                }
            }
        }

        if (did_something) {
            printk("Memory map entry was not page aligned\n");
            continue;
        }
    } while (did_something);

    printk("Memory map fixup complete\n");
}

void mmu_init(void)
{
    mmu_fixup_mem_map(phys_mem_map);

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
    highest_usable >>= PAGE_SIZE_BIT;
    printk("Usable pages = %lu (%luMB) range_pages=%ld\n", usable_pages,
           usable_pages >> (20 - PAGE_SIZE_BIT), highest_usable);

    /// Physical page allocation map
    /// Consists of array of entries, one per physical page
    /// Each slot contains the index of the next slot,
    /// which forms a singly-linked list of pages.
    /// Free pages are linked into a chain, allowing for O(1)
    /// performance allocation and freeing of physical pages.
    ///
    /// 2^32 pages is 2^(32+12-40) bytes = 16TB
    ///
    /// This results in a 0.098% overhead for the physical page
    /// allocation map at 4 bytes per 4KB.
    ///


}

void mmu_set_fsgsbase(void *fs_base, void *gs_base)
{
    msr_set(MSR_FSBASE, (uint64_t)fs_base);
    msr_set(MSR_GSBASE, (uint64_t)gs_base);
}
