#include "mmu.h"
#include "control_regs.h"
#include "printk.h"
#include "time.h"
#include "string.h"

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
#define PT_KBASEADDR   (0x7F8000000000UL)
#define PT_UBASEADDR   (0x7F0000000000UL)

// The number of pte_t entries at each level
#define PT3_ENTRIES     (0x400000000UL)
#define PT2_ENTRIES     (0x2000000UL)
#define PT1_ENTRIES     (0x10000UL)
#define PT0_ENTRIES     (0x80UL)

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

static pte_t * const ptk_base[4] = {
    PT0_PTR(PT_KBASEADDR),
    PT1_PTR(PT_KBASEADDR),
    PT2_PTR(PT_KBASEADDR),
    PT3_PTR(PT_KBASEADDR)
};

static pte_t * const ptu_base[4] = {
    PT0_PTR(PT_UBASEADDR),
    PT1_PTR(PT_UBASEADDR),
    PT2_PTR(PT_UBASEADDR),
    PT3_PTR(PT_UBASEADDR)
};

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
static size_t mmu_fixup_mem_map(physmem_range_t *list)
{
    int did_something;
    size_t usable_count;

    // Might need multiple passes to fully fixup the list
    do {
        did_something = 0;

        // Make invalid and zero size entries get removed
        for (size_t i = 1; i < phys_mem_map_count; ++i) {
            if (list[i].size == 0 && !list[i].valid) {
                list[i].base = ~0UL;
                list[i].size = ~0UL;
            }
        }

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

                did_something = 1;

                // Compute bounds of prev
                uint64_t prev_st = list[i-1].base;
                uint64_t prev_en = prev_st + list[i-1].size;

                // Compute bounds of this
                uint64_t this_st = list[i].base;
                uint64_t this_en = this_st + list[i].size;

                // Start at lowest bound, end at highest bound
                uint64_t st = prev_st < this_st ? prev_st : this_st;
                uint64_t en = prev_en > this_en ? prev_en : this_en;

                if (list[i-1].type == list[i].type) {
                    // Both same type,
                    // make one cover combined range

                    printk("Combining overlapping ranges of same type\n");

                    // Remove previous entry
                    list[i-1].base = ~0UL;
                    list[i-1].size = ~0UL;

                    list[i].base = st;
                    list[i].size = en - st;

                    break;
                } else if (list[i-1].type == PHYSMEM_TYPE_NORMAL) {
                    // This entry takes precedence over prev entry

                    if (st < this_st && en > this_en) {
                        // Punching a hole in the prev entry

                        printk("Punching hole in memory range\n");

                        // Reduce size of prev one to not overlap
                        list[i-1].size = this_st - prev_st;

                        // Make new entry with normal memory after this one
                        // Sort will put it in the right position later
                        list[phys_mem_map_count].base = this_en;
                        list[phys_mem_map_count].size = en - this_en;
                        list[phys_mem_map_count].type = PHYSMEM_TYPE_NORMAL;
                        list[phys_mem_map_count].valid = 1;
                        ++phys_mem_map_count;

                        break;
                    } else if (st < this_st && en >= this_en) {
                        // Prev entry partially overlaps this entry

                        printk("Correcting overlap\n");

                        list[i-1].size = this_st - prev_st;
                    }
                } else {
                    // Prev entry takes precedence over this entry

                    if (st < this_st && en > this_en) {
                        // Prev entry eliminates this entry
                        list[i].base = ~0UL;
                        list[i].size = ~0UL;

                        printk("Removing completely overlapped range\n");
                    } else if (st < this_st && en >= this_en) {
                        // Prev entry partially overlaps this entry

                        printk("Correcting overlap\n");

                        list[i].base = prev_en;
                        list[i].size = this_en - prev_en;
                    }
                }
            }
        }

        if (did_something) {
            printk("Memory map overlap fixed up\n");
            continue;
        }

        usable_count = 0;

        // Fixup page alignment
        for (size_t i = 0; i < phys_mem_map_count; ++i) {
            if (list[i].type == PHYSMEM_TYPE_NORMAL) {
                ++usable_count;

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

    return usable_count;
}

#define alloca __builtin_alloca

static uint64_t init_take_page(physmem_range_t *ranges, size_t *usable_ranges)
{
    if (*usable_ranges == 0)
        return ~0L;

    physmem_range_t *last_range = ranges + *usable_ranges - 1;
    uint64_t addr = last_range->base + last_range->size - PAGE_SIZE;

    // Take a page off the size of the range
    last_range->size -= PAGE_SIZE;

    // If range exhausted, switch to next range
    if (last_range->size == 0)
        --*usable_ranges;

    return addr;
}

static void path_from_addr(unsigned *path, uint64_t addr)
{
    unsigned slot = 0;
    for (uint8_t shift = 39; shift >= 12; shift -= 9)
        path[slot++] = (addr >> shift) & 0x1FF;
}

// Returns the linear addresses of the page tables for
// the given path
static void pages_from_path(pte_t **pte, unsigned *path)
{
    uint64_t base = path[0] < 0x80
            ? PT_UBASEADDR
            : PT_KBASEADDR;

    uint64_t page_index =
            ((uint64_t)path[0] << (9 * 3)) +
            ((uint64_t)path[1] << (9 * 2)) +
            ((uint64_t)path[2] << (9 * 1)) +
            ((uint64_t)path[3] << (9 * 0));

    uint64_t indices[4];

    indices[0] = (page_index >> (9 * 3)) & (PT0_ENTRIES-1);
    indices[1] = (page_index >> (9 * 2)) & (PT1_ENTRIES-1);
    indices[2] = (page_index >> (9 * 1)) & (PT2_ENTRIES-1);
    indices[3] = (page_index >> (9 * 0)) & (PT3_ENTRIES-1);

    pte[0] = PT0_PTR(base) + indices[0];
    pte[1] = PT1_PTR(base) + indices[1];
    pte[2] = PT2_PTR(base) + indices[2];
    pte[3] = PT3_PTR(base) + indices[3];
}

static pte_t *init_find_aliasing_pte(void)
{
    pte_t *pte = (pte_t*)(cpu_get_page_directory() & PTE_ADDR);
    unsigned path[4];
    path_from_addr(path, 0x800000000000 - PAGE_SIZE);

    for (unsigned level = 0; level < 3; ++level)
        pte = (pte_t*)(pte[path[level]] & PTE_ADDR);

    return pte + path[3];
}

static pte_t *init_map_aliasing_pte(pte_t *aliasing_pte, uint64_t addr)
{
    static uint64_t cur_alias_addr;

    uint64_t alias_addr = 0x800000000000 - PAGE_SIZE;

    if (cur_alias_addr != addr) {
        *aliasing_pte = ((*aliasing_pte & ~PTE_ADDR) | addr) |
                PTE_PRESENT | PTE_WRITABLE;
        cpu_invalidate_page(alias_addr);
        cur_alias_addr = addr;
    }

    return (pte_t*)alias_addr;
}

// Pass phys_addr=~0UL to allocate a new table
static void init_create_pt(
        uint64_t root_physaddr,
        pte_t *aliasing_pte,
        uint64_t linear_addr,
        uint64_t phys_addr,
        physmem_range_t *ranges,
        size_t *usable_ranges)
{
    printk("Creating page table for %lx...", linear_addr);

    // Path to page table entry as page indices
    unsigned path[4];
    path_from_addr(path, linear_addr);

    unsigned levels = (phys_addr == ~0UL) ? 4 : 3;

    pte_t *iter;

    // Map aliasing page to point to new page
    iter = init_map_aliasing_pte(aliasing_pte, root_physaddr);

    uint64_t addr;
    for (unsigned level = 0; level < levels; ++level) {
        addr = iter[path[level]] & PTE_ADDR;

        if (!addr) {
            // Allocate a new page table
            addr = init_take_page(ranges, usable_ranges);

            // Update current page table to point to new page
            iter[path[level]] = addr | (PTE_PRESENT | PTE_WRITABLE);

            // Map aliasing page to point to new page
            iter = init_map_aliasing_pte(aliasing_pte, addr);

            // Clear new page table
            aligned16_memset(iter, 0, PAGE_SIZE);
        } else {
            // Descend into next page table
            iter = init_map_aliasing_pte(aliasing_pte, addr);
        }
    }

    if (levels == 3 && !(iter[path[3]] & PTE_PRESENT)) {
        printk("[%u] at %lx\n", path[3], phys_addr);
        iter[path[3]] = (phys_addr & PTE_ADDR) |
            (PTE_PRESENT | PTE_WRITABLE);
    } else {
        printk("already existed\n");
    }
}

void mmu_init(void)
{
    size_t usable_ranges = mmu_fixup_mem_map(phys_mem_map);

    // Make private copy of just usable ranges for modification
    physmem_range_t *ranges = alloca(
                sizeof(physmem_range_t) * usable_ranges);
    for (physmem_range_t *ranges_in = phys_mem_map, *ranges_out = ranges;
         ranges_in < phys_mem_map + phys_mem_map_count;
         ++ranges_in) {
        if (ranges_in->type == PHYSMEM_TYPE_NORMAL)
            *ranges_out++ = *ranges_in;
    }

    uint64_t usable_pages = 0;
    uint64_t highest_usable = 0;
    for (physmem_range_t *mem = ranges;
         mem < ranges + usable_ranges; ++mem) {
        printk("Memory: addr=%lx size=%lx type=%x\n",
               mem->base, mem->size, mem->type);

        if (mem->base >= 0x100000) {
            uint64_t end = mem->base + mem->size;

            if (PHYSMEM_TYPE_NORMAL) {
                usable_pages += mem->size >> PAGE_SIZE_BIT;

                if (highest_usable < end)
                    highest_usable = end;
            }
        }
    }

    // Exclude pages below 1MB line from physical page allocation map
    highest_usable -= 0x100000;

    // Compute number of slots needed
    highest_usable >>= PAGE_SIZE_BIT;
    printk("Usable pages = %lu (%luMB) range_pages=%ld\n", usable_pages,
           usable_pages >> (20 - PAGE_SIZE_BIT), highest_usable);

    //
    // Alias the existing page tables into the appropriate addresses

    pte_t *aliasing_pte = init_find_aliasing_pte();

    // Create the new root

    // Get a page
    uint64_t root_physaddr = init_take_page(ranges, &usable_ranges);

    // Map page
    pte_t *root = init_map_aliasing_pte(aliasing_pte, root_physaddr);

    // Clear page
    aligned16_memset(root, 0, PAGE_SIZE);

    init_create_pt(root_physaddr, aliasing_pte,
                   (uint64_t)PT0_PTR(PT_KBASEADDR),
                   root_physaddr,
                   ranges, &usable_ranges);

    unsigned path[4];
    uint64_t phys_addr[4];
    pte_t *pt;

    phys_addr[0] = cpu_get_page_directory() & PTE_ADDR;

    for (path[0] = 0; path[0] < 512; ++path[0]) {
        pt = init_map_aliasing_pte(aliasing_pte, phys_addr[0]);

        // Skip if PT not present
        if ((pt[path[0]] & PTE_PRESENT) == 0)
            continue;

        // Get address of next level PT
        phys_addr[1] = pt[path[0]] & PTE_ADDR;

        for (path[1] = 0; path[1] < 512; ++path[1]) {
            pt = init_map_aliasing_pte(aliasing_pte, phys_addr[1]);

            // Skip if PT not present
            if ((pt[path[1]] & PTE_PRESENT) == 0)
                continue;

            // Get address of next level PT
            phys_addr[2] = pt[path[1]] & PTE_ADDR;

            for (path[2] = 0; path[2] < 512; ++path[2]) {
                pt = init_map_aliasing_pte(aliasing_pte, phys_addr[2]);

                // Skip if PT not present
                if ((pt[path[2]] & PTE_PRESENT) == 0)
                    continue;

                // Get address of next level PT
                phys_addr[3] = pt[path[2]] & PTE_ADDR;

                for (path[3] = 0; path[3] < 512; ++path[3]) {
                    pt = init_map_aliasing_pte(aliasing_pte, phys_addr[3]);

                    pte_t pte = pt[path[3]];

                    // Skip if PT not present
                    if ((pte & PTE_PRESENT) == 0)
                        continue;

                    init_create_pt(root_physaddr, aliasing_pte,
                            ((uint64_t)path[0] << (9 * 3 + 12)) |
                            ((uint64_t)path[1] << (9 * 2 + 12)) |
                            ((uint64_t)path[2] << (9 * 1 + 12)) |
                            ((uint64_t)path[3] << (9 * 0 + 12)),
                            pte & PTE_ADDR,
                            ranges, &usable_ranges);

                    pte_t *virtual_tables[4];
                    pages_from_path(virtual_tables, path);

                    for (unsigned i = 0; i < 4; ++i) {
                        init_create_pt(root_physaddr, aliasing_pte,
                                       (uint64_t)virtual_tables[i] & PTE_ADDR,
                                       phys_addr[i],
                                       ranges, &usable_ranges);
                    }

                    printk("Virtual tables for 0x%lx:\n",
                           ((uint64_t)path[0] << (9 * 3 + 12)) +
                           ((uint64_t)path[1] << (9 * 2 + 12)) +
                           ((uint64_t)path[2] << (9 * 1 + 12)) +
                           ((uint64_t)path[3] << (9 * 0 + 12)));
                    printk("  %012lx %012lx %012lx %012lx\n",
                           (uint64_t)virtual_tables[0],
                           (uint64_t)virtual_tables[1],
                           (uint64_t)virtual_tables[2],
                           (uint64_t)virtual_tables[3]);
                }
            }
        }
    }

    pt = init_map_aliasing_pte(aliasing_pte, root_physaddr);
    cpu_set_page_directory(root_physaddr);
}
