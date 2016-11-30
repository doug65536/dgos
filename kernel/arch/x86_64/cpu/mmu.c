#include "mmu.h"
#include "mm.h"
#include "control_regs.h"
#include "printk.h"
#include "time.h"
#include "string.h"
#include "atomic.h"
#include "bios_data.h"
#include "callout.h"
#include "likely.h"

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

// Assigned usage of available bits
#define PTE_EX_PHYSICAL_BIT (PTE_AVAIL1_BIT+0)
#define PTE_EX_LOCKED_BIT   (PTE_AVAIL1_BIT+1)

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

// Assigned usage of available PTE bits
#define PTE_EX_PHYSICAL     (1ULL << PTE_EX_PHYSICAL_BIT)
#define PTE_EX_LOCKED       (1ULL << PTE_EX_LOCKED_BIT)

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

typedef uintptr_t physaddr_t;
typedef uintptr_t linaddr_t;

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
#define PT_MAX_ADDR    (0x800000000000UL)

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
#define PT3_PTR(base)   ((pte_t*)(base))
#define PT2_PTR(base)   (PT3_PTR(base) + PT2_BASE)
#define PT1_PTR(base)   (PT3_PTR(base) + PT1_BASE)
#define PT0_PTR(base)   (PT3_PTR(base) + PT0_BASE)

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

// Simple physical memory allocator
physmem_range_t ranges[64];
size_t usable_ranges;

physaddr_t root_physaddr;

// Bitmask of available aliasing ptes (top one taken already)
static uint64_t volatile apte_map = 0x8000000000000000UL;


unsigned *phys_alloc;
unsigned phys_alloc_count;

extern char ___init_brk[];
extern uint64_t ___top_physaddr;
static linaddr_t volatile linear_allocator = (linaddr_t)___init_brk;
static uint64_t volatile phys_next_free;

static void mmu_free_phys(physaddr_t addr)
{
    uint64_t index = ((uint64_t)addr - 0x100000) >> PAGE_SIZE_BIT;
    unsigned old_next = phys_next_free;
    for (;;) {
        phys_alloc[index] = old_next;

        unsigned cur_next = atomic_cmpxchg(
                    &phys_next_free, old_next, index);

        if (cur_next == old_next)
            break;

        old_next = cur_next;

        pause();
    }
}

static physaddr_t mmu_alloc_phys(void)
{
    unsigned index = phys_next_free;
    for (;;) {
        unsigned new_next = phys_alloc[index];

        unsigned cur_next = atomic_cmpxchg(
                    &phys_next_free, index, new_next);

        if (cur_next == index) {
            phys_alloc[index] = 0;
            return ((physaddr_t)index << PAGE_SIZE_BIT) + 0x100000;
        }

        index = cur_next;

        pause();
    }
}

//
// Path to PTE

static void path_from_addr(unsigned *path, linaddr_t addr)
{
    unsigned slot = 0;
    for (uint8_t shift = 39; shift >= 12; shift -= 9)
        path[slot++] = (addr >> shift) & 0x1FF;
}

static void path_inc(unsigned *path)
{
    // Branchless algorithm

    uint64_t n =
            ((linaddr_t)path[0] << (9 * 3)) |
            ((linaddr_t)path[1] << (9 * 2)) |
            ((linaddr_t)path[2] << (9 * 1)) |
            (linaddr_t)(path[3]);

    ++n;

    path[0] = (unsigned)(n >> (9 * 3)) & 511;
    path[1] = (unsigned)(n >> (9 * 2)) & 511;
    path[2] = (unsigned)(n >> (9 * 1)) & 511;
    path[3] = (unsigned)n & 511;
}

// Returns the linear addresses of the page tables for
// the given path
static void pte_from_path(pte_t **pte, unsigned *path)
{
    linaddr_t base = path[0] >= 0x80
            ? PT_KBASEADDR
            : PT_UBASEADDR;

    uint64_t page_index =
            ((uint64_t)(path[0]) << (9 * 3)) +
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

static int mmu_path_present(unsigned *path, pte_t **optional_pte_ret)
{
    pte_t *pteptr[4];

    if (!optional_pte_ret)
        optional_pte_ret = pteptr;

    pte_from_path(optional_pte_ret, path);

    return (*optional_pte_ret[0] & PTE_PRESENT) &&
            (*optional_pte_ret[1] & PTE_PRESENT) &&
            (*optional_pte_ret[2] & PTE_PRESENT) &&
            (*optional_pte_ret[3] & PTE_PRESENT);
}

//static int mmu_linaddr_present(linaddr_t addr, pte_t **optional_pte_ret)
//{
//    unsigned path[4];
//    path_from_addr(path, addr);
//    return mmu_path_present(path, optional_pte_ret);
//}

//
// Aliasing page allocator

// Maps the specified physical address and returns a pointer
// which you can use to modify that physical address
// If the address is ~0UL then returns pointer to PTE itself
static pte_t *take_apte(physaddr_t address)
{
    pte_t *result;
    uint64_t old_map = apte_map;
    for (;;) {
        if (~old_map) {
            // __builtin_ctzl returns number of trailing zeros
            unsigned first_available = __builtin_ctzl(~old_map);
            uint64_t new_map = old_map | (1L << first_available);

            if (atomic_cmpxchg_uint64(
                        &apte_map,
                        old_map,
                        new_map) == old_map) {
                // Successfully acquired entry

                linaddr_t linaddr = 0x800000000000UL -
                        (64 << PAGE_SIZE_BIT) +
                        (first_available << PAGE_SIZE_BIT);

                // Locate pte
                unsigned path[4];
                path_from_addr(path, linaddr);
                pte_t *pteptr[4];

                pte_from_path(pteptr, path);

                result = pteptr[3];

                if (address != ~0UL) {
                    // Modify pte
                    *result = (address & PTE_ADDR) |
                            (PTE_PRESENT | PTE_WRITABLE);

                    // Calculate linear address for pte
                    result = (pte_t*)
                            (PT_MAX_ADDR -
                             (64  << PAGE_SIZE_BIT) +
                             (first_available << PAGE_SIZE_BIT));

                    // Flush tlb for pte address range
                    cpu_invalidate_page((linaddr_t)result);

                    // Return pointer to linear address
                    // which maps physical address
                } else {
                    // Return pointer to the PTE itself
                    *result |= PTE_PRESENT | PTE_WRITABLE;
                    cpu_invalidate_page(linaddr);
                }

                return result;
            }
        }
        pause();
    }
}

static void release_apte(pte_t *address)
{
    int bit;
    if (address >= PT3_PTR(PT_KBASEADDR) + PT3_ENTRIES - 64 &&
            address < PT3_PTR(PT_KBASEADDR) + PT3_ENTRIES) {
        // Address refers to a page table entry
        bit = address - (PT3_PTR(PT_KBASEADDR) + PT3_ENTRIES - 64);
    } else {
        // Address refers to a mapped page table entry
        bit = 64 - ((PT_MAX_ADDR - (linaddr_t)address) >> PAGE_SIZE_BIT);
    }
    atomic_and_uint64(&apte_map, ~(1UL << bit));
}

static void mmu_mem_map_swap(physmem_range_t *a, physmem_range_t *b)
{
    physmem_range_t temp = *a;
    *a = *b;
    *b = temp;
}

//
// Physical memory allocation map processing

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
                physaddr_t prev_st = list[i-1].base;
                physaddr_t prev_en = prev_st + list[i-1].size;

                // Compute bounds of this
                physaddr_t this_st = list[i].base;
                physaddr_t this_en = this_st + list[i].size;

                // Start at lowest bound, end at highest bound
                physaddr_t st = prev_st < this_st ? prev_st : this_st;
                physaddr_t en = prev_en > this_en ? prev_en : this_en;

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

                physaddr_t st = list[i].base;
                physaddr_t en = list[i].base + list[i].size;

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

//
// Initialization page allocator

static physaddr_t init_take_page(void)
{
    if (likely(usable_ranges == 0))
        return mmu_alloc_phys();

    physmem_range_t *last_range = ranges + usable_ranges - 1;
    physaddr_t addr = last_range->base + last_range->size - PAGE_SIZE;

    // Take a page off the size of the range
    last_range->size -= PAGE_SIZE;

    // If range exhausted, switch to next range
    if (last_range->size == 0)
        --usable_ranges;

    return addr;
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

//
// Aliasing page table mapping

static pte_t *mm_map_aliasing_pte(pte_t *aliasing_pte, physaddr_t addr)
{
    uint64_t linaddr;

    if ((linaddr_t)aliasing_pte >= PT_UBASEADDR) {
        uint64_t pt3_index = aliasing_pte - PT3_PTR(PT_KBASEADDR);
        linaddr = 0x400000000000 + (pt3_index << PAGE_SIZE_BIT);
    } else {
        linaddr = 0x800000000000 - PAGE_SIZE;
    }

    *aliasing_pte = (*aliasing_pte & ~PTE_ADDR) |
            (addr & PTE_ADDR) |
            (PTE_PRESENT | PTE_WRITABLE);

    cpu_invalidate_page(linaddr);

    return (pte_t*)linaddr;
}

static pte_t *init_map_aliasing_pte(pte_t *aliasing_pte, physaddr_t addr)
{
    return mm_map_aliasing_pte(aliasing_pte, addr);
}

//
// Page table creation

// Pass phys_addr=~0UL to allocate a new table
static void init_create_pt(
        physaddr_t root_physaddr,
        pte_t *aliasing_pte,
        linaddr_t linear_addr,
        physaddr_t phys_addr,
        physaddr_t *pt_physaddr,
        pte_t page_flags)
{
    //printk("Creating page table for %lx...", linear_addr);

    // Path to page table entry as page indices
    unsigned path[4];
    path_from_addr(path, linear_addr);

    unsigned levels = (phys_addr == ~0UL) ? 4 : 3;

    pte_t *iter;

    // Map aliasing page to point to new page
    iter = mm_map_aliasing_pte(aliasing_pte, root_physaddr);

    if (pt_physaddr)
        pt_physaddr[0] = root_physaddr;

    physaddr_t addr;
    for (unsigned level = 0; level < levels; ++level) {
        addr = iter[path[level]] & PTE_ADDR;

        if (!addr) {
            // Allocate a new page table
            addr = init_take_page();

            // Update current page table to point to new page
            iter[path[level]] = addr | page_flags;

            // Map aliasing page to point to new page
            iter = init_map_aliasing_pte(aliasing_pte, addr);

            // Clear new page table
            aligned16_memset(iter, 0, PAGE_SIZE);
        } else {
            // Descend into next page table
            iter = init_map_aliasing_pte(aliasing_pte, addr);
        }

        if (pt_physaddr)
            pt_physaddr[level+1] = addr;
    }

    if (levels == 3 && !(iter[path[3]] & PTE_PRESENT)) {
        //printk("[%u] at %lx\n", path[3], phys_addr);
        iter[path[3]] = (phys_addr & PTE_ADDR) |
            page_flags;
        cpu_invalidate_page(linear_addr);
    } else {
        //printk("already existed\n");
    }
}

static void mmu_map_page(
        linaddr_t addr,
        physaddr_t physaddr, pte_t flags)
{
    // Read root physical address from page tables
    physaddr_t root_physaddr = cpu_get_page_directory();
    //*PT0_PTR(PT_KBASEADDR) & PTE_ADDR;

    // Get a free aliasing PTE
    pte_t *aliasing_pte = take_apte(~0UL);

    physaddr_t pt_physaddr[4];
    init_create_pt(root_physaddr, aliasing_pte,
                   addr, physaddr,
                   pt_physaddr,
                   flags);

    unsigned path[4];
    path_from_addr(path, addr);

    pte_t *pte_linaddr[4];
    pte_from_path(pte_linaddr, path);

    // Map the page tables for the region
    for (unsigned i = 0; i < 4; ++i) {
        init_create_pt(root_physaddr, aliasing_pte,
                       (linaddr_t)pte_linaddr[i],
                       pt_physaddr[i], 0,
                       PTE_PRESENT | PTE_WRITABLE);
    }

    release_apte(aliasing_pte);
}

//
// Initialization

void mmu_init(int ap)
{
    if (ap) {
        cpu_set_page_directory(root_physaddr);
        return;
    }

    usable_ranges = mmu_fixup_mem_map(phys_mem_map);

    if (usable_ranges > countof(ranges)) {
        printk("Physical memory is incredibly fragmented!\n");
        usable_ranges = countof(ranges);
    }

    for (physmem_range_t *ranges_in = phys_mem_map, *ranges_out = ranges;
         ranges_in < phys_mem_map + phys_mem_map_count;
         ++ranges_in) {
        if (ranges_in->type == PHYSMEM_TYPE_NORMAL)
            *ranges_out++ = *ranges_in;

        // Cap
        if (ranges_out >= ranges + usable_ranges)
            break;
    }

    size_t usable_pages = 0;
    physaddr_t highest_usable = 0;
    for (physmem_range_t *mem = ranges;
         mem < ranges + usable_ranges; ++mem) {
        printk("Memory: addr=%lx size=%lx type=%x\n",
               mem->base, mem->size, mem->type);

        if (mem->base >= 0x100000) {
            physaddr_t end = mem->base + mem->size;

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
    root_physaddr = init_take_page();

    // Map page
    pte_t *root = init_map_aliasing_pte(aliasing_pte, root_physaddr);

    // Clear page
    aligned16_memset(root, 0, PAGE_SIZE);

    init_create_pt(root_physaddr, aliasing_pte,
                   (linaddr_t)PT0_PTR(PT_KBASEADDR),
                   root_physaddr,
                   0,
                   PTE_PRESENT | PTE_WRITABLE);

    unsigned path[4];
    physaddr_t phys_addr[4];
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

                    physaddr_t pt_physaddr[4];

                    init_create_pt(root_physaddr, aliasing_pte,
                            ((uint64_t)path[0] << (9 * 3 + 12)) |
                            ((uint64_t)path[1] << (9 * 2 + 12)) |
                            ((uint64_t)path[2] << (9 * 1 + 12)) |
                            ((uint64_t)path[3] << (9 * 0 + 12)),
                            pte & PTE_ADDR,
                            pt_physaddr,
                            PTE_PRESENT | PTE_WRITABLE);

                    pte_t *virtual_tables[4];
                    pte_from_path(virtual_tables, path);

                    for (unsigned i = 0; i < 4; ++i) {
                        init_create_pt(root_physaddr, aliasing_pte,
                                       (linaddr_t)virtual_tables[i],
                                       pt_physaddr[i],
                                       0,
                                       PTE_PRESENT | PTE_WRITABLE);
                    }
                }
            }
        }
    }

    //pt = init_map_aliasing_pte(aliasing_pte, root_physaddr);
    cpu_set_page_directory(root_physaddr);

    // Make zero page not present to catch null pointers
    *PT3_PTR(PT_UBASEADDR) = 0;
    cpu_invalidate_page(0);

    phys_alloc_count = highest_usable;
    phys_alloc = mmap(0, phys_alloc_count * sizeof(unsigned),
         PROT_READ | PROT_WRITE, 0, -1, 0);


    printk("Building physical memory free list\n");

    memset(phys_alloc, 0, phys_alloc_count * sizeof(unsigned));

    // Put all of the remaining physical memory into the free list
    uint64_t free_count = 0;
    for (; ; ++free_count) {
        physaddr_t addr = init_take_page();
        if (addr >= ___top_physaddr && addr < 0x8000000000000000UL) {
            addr -= 0x100000;
            addr >>= PAGE_SIZE_BIT;
            phys_alloc[addr] = phys_next_free;
            phys_next_free = addr;
        } else {
            break;
        }
    }

    // Start using physical memory allocator
    usable_ranges = 0;

    printk("%lu pages free (%luMB)\n",
           free_count,
           free_count >> (20 - PAGE_SIZE_BIT));

    callout_call('V');

    zero_page = mmap(
                0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_PHYSICAL, -1, 0);
}

static size_t round_up(size_t n)
{
    return (n + PAGE_MASK) & -PAGE_SIZE;
}

//
// Linear address allocator

static linaddr_t take_linear(size_t size)
{
    if (size > 0) {
        // Allocate 2 extra pages as guard pages
        linaddr_t addr = atomic_xadd_uint64(
                    &linear_allocator,
                    round_up(size) + PAGE_SIZE * 2);

        return addr + PAGE_SIZE;
    }
    return 0;
}

//
// Public API

void *mmap(
        void *addr,
        size_t len,
        int prot,
        int flags,
        int fd,
        off_t offset)
{
    // Bomb out on unsupported stuff, for now
    if (fd != -1 || offset != 0 || !len)
        return 0;

    pte_t page_flags = PTE_PRESENT;

    if (likely(prot & PROT_WRITE))
        page_flags |= PTE_WRITABLE;

    // fixme check cpuid
    //if (!(prot & PROT_EXEC))
    //    flags |= PTE_NX;

    uint64_t misalignment = 0;

    if (unlikely(flags & MAP_PHYSICAL)) {
        misalignment = (uint64_t)addr & PAGE_MASK;
        len += misalignment;
    }

    linaddr_t linear_addr = take_linear(len);

    for (size_t ofs = 0; ofs < len + PAGE_MASK; ofs += PAGE_SIZE)
    {
        if (likely(!(flags & MAP_PHYSICAL))) {
            // Allocate normal memory
            physaddr_t page = init_take_page();
            mmu_map_page(linear_addr + ofs, page,
                         PTE_PRESENT | PTE_WRITABLE);
        } else {
            // addr is a physical address, caller uses
            // returned linear address to access it
            mmu_map_page(linear_addr + ofs,
                         (physaddr_t)addr + ofs,
                         PTE_PRESENT | PTE_WRITABLE |
                         PTE_PCD | PTE_PWT |
                         PTE_EX_PHYSICAL);
        }
    }

    return (void*)(linear_addr + misalignment);
}

int munmap(void *addr, size_t len)
{
    linaddr_t a = (linaddr_t)addr;

    uint64_t misalignment = 0;

    misalignment = a & PAGE_MASK;
    a -= misalignment;
    len += misalignment;

    unsigned path[4];
    path_from_addr(path, a);

    pte_t *pteptr[4];

    for (size_t ofs = 0; ofs < len + PAGE_MASK; ofs += PAGE_SIZE)
    {
        if (!mmu_path_present(path, pteptr))
            return -1;

        pte_t pte = *pteptr[3];

        if (likely(!(pte & PTE_EX_PHYSICAL))) {
            physaddr_t addr = pte & PTE_ADDR;

            mmu_free_phys(addr);
        }

        *pteptr[3] = 0;

        cpu_invalidate_page(a);

        a += PAGE_SIZE;
        path_inc(path);
    }

    return 0;
}
