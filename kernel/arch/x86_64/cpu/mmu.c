#include "mmu.h"
#include "mm.h"
#include "control_regs.h"
#include "interrupts.h"
#include "irq.h"
#include "apic.h"
#include "printk.h"
#include "time.h"
#include "string.h"
#include "atomic.h"
#include "bios_data.h"
#include "callout.h"
#include "likely.h"
#include "assert.h"
#include "cpuid.h"
#include "thread_impl.h"
#include "threadsync.h"
#include "rbtree.h"
#include "idt.h"

#define DEBUG_ADDR_ALLOC    0
#define DEBUG_PHYS_ALLOC    0
#define DEBUG_PAGE_TABLES   0
#define DEBUG_PAGE_FAULT    0

// Intel manual, page 2786

// The entries of the 4 levels of page tables are named:
//  PML4E (maps 512 512GB regions)
//  PDPTE (maps 512 1GB regions)
//  PDE   (maps 512 2MB regions)
//  PTE   (maps 512 4KB regions)

// 4KB pages
#define PAGE_SIZE_BIT       12
#define PAGE_SIZE           (1UL << PAGE_SIZE_BIT)
#define PAGE_MASK           (PAGE_SIZE - 1UL)

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
#define PTE_AVAIL2_BIT      52

// Assigned usage of available bits
#define PTE_EX_PHYSICAL_BIT (PTE_AVAIL1_BIT+0)
#define PTE_EX_LOCKED_BIT   (PTE_AVAIL1_BIT+1)

// Size of multi-bit fields
#define PTE_PK_BITS         4
#define PTE_ADDR_BITS       40
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
typedef uintptr_t pte_t;

typedef uintptr_t physaddr_t;
typedef uintptr_t linaddr_t;

// The page tables are mapped at
//  0x7f0000000000

// Linear addresses
#define PT_BASEADDR   (0x7F0000000000UL)
#define PT_MAX_ADDR    (0x800000000000UL)

// The number of pte_t entries at each level
// Total data 275,415,828,480 bytes
#define PT3_ENTRIES     (0x800000000UL)
#define PT2_ENTRIES     (0x4000000UL)
#define PT1_ENTRIES     (0x20000UL)
#define PT0_ENTRIES     (0x100UL)

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

// Memory map
physmem_range_t mem_ranges[64];
size_t usable_mem_ranges;

physaddr_t root_physaddr;

// Bitmask of available aliasing ptes (top one taken already)
static uint64_t volatile apte_map = 0x8000000000000000UL;

uint32_t *phys_alloc;
unsigned phys_alloc_count;

extern char ___init_brk[];
extern uintptr_t ___top_physaddr;
static linaddr_t volatile linear_allocator = (linaddr_t)___init_brk;

// Maintain two free page chains,
// because some hardware requires 32-bit memory addresses
// [0] is the chain >= 4GB
// [1] is the chain < 4GB
static uint64_t volatile phys_next_free[2];

// Incremented every time the page tables are changed
// Used to detect lazy TLB shootdown
static uint64_t volatile mmu_seq;

// Free address space management
static mutex_t free_addr_lock;
static rbtree_t *free_addr_by_size;
static rbtree_t *free_addr_by_addr;

static linaddr_t take_linear(size_t size);
static void release_linear(linaddr_t addr, size_t size);

static int take_linear_cmp_key(
        rbtree_kvp_t const *lhs,
        rbtree_kvp_t const *rhs,
        void *p);

static int take_linear_cmp_both(
        rbtree_kvp_t const *lhs,
        rbtree_kvp_t const *rhs,
        void *p);

static uint64_t volatile page_fault_count;

static int64_t volatile free_page_count;

static void mmu_free_phys(physaddr_t addr)
{
    uint64_t volatile *chain = phys_next_free + (addr < 0x100000000UL);
    uint64_t index = (addr - 0x100000UL) >> PAGE_SIZE_BIT;
    uint64_t old_next = *chain;
    for (;;) {
        assert(index < phys_alloc_count);
        phys_alloc[index] = (int32_t)old_next;
        uint64_t next_version = (old_next & 0xFFFFFFFF00000000UL) +
                0x100000000UL;

        uint64_t cur_next = atomic_cmpxchg(
                    chain,
                    old_next,
                    index | next_version);

        if (cur_next == old_next) {
            atomic_inc_int64(&free_page_count);

#if DEBUG_PHYS_ALLOC
            printdbg("Free phys page addr=%lx\n", addr);
#endif
            break;
        }

        old_next = cur_next;

        pause();
    }
}

static physaddr_t mmu_alloc_phys(int low)
{
    // Use low memory immediately if high
    // memory is exhausted
    low |= (phys_next_free[0] == 0);

    uint64_t volatile *chain = phys_next_free + !!low;

    uint64_t index_version = *chain;
    for (;;) {
        uint64_t next_version = (index_version & 0xFFFFFFFF00000000UL) +
                0x100000000UL;
        uint32_t index = (uint32_t)index_version;
        uint64_t new_next = index ? phys_alloc[index] : 0;

        assert(index < phys_alloc_count);

        new_next |= next_version;

        if (!new_next) {
            // Resort to low memory pages if high memory exhausted
            if (!low)
                return mmu_alloc_phys(1);

            // Out of memory
            return 0;
        }

        assert((new_next & 0xFFFFFFFFUL) < phys_alloc_count);

        uint64_t cur_next = atomic_cmpxchg(chain, index_version, new_next);

        if (cur_next == index_version) {
            atomic_dec_int64(&free_page_count);

            phys_alloc[index] = 0;
            physaddr_t addr = (((physaddr_t)index) <<
                               PAGE_SIZE_BIT) + 0x100000;

#if DEBUG_PHYS_ALLOC
            printdbg("Allocated phys page addr=%lx\n", addr);
#endif

            return addr;
        }

        index_version = cur_next;

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

    uintptr_t n =
            ((linaddr_t)path[0] << (9 * 3)) |
            ((linaddr_t)path[1] << (9 * 2)) |
            ((linaddr_t)path[2] << (9 * 1)) |
            (linaddr_t)(path[3]);

    ++n;

    path[0] = (unsigned)(n >> (9 * 3)) & 0x1FF;
    path[1] = (unsigned)(n >> (9 * 2)) & 0x1FF;
    path[2] = (unsigned)(n >> (9 * 1)) & 0x1FF;
    path[3] = (unsigned)n & 0x1FF;
}

// Returns the linear addresses of the page tables for
// the given path
static void pte_from_path(pte_t **pte, unsigned *path)
{
    linaddr_t base = PT_BASEADDR;

    uintptr_t page_index =
            (((uintptr_t)path[0]) << (9 * 3)) +
            (((uintptr_t)path[1]) << (9 * 2)) +
            (((uintptr_t)path[2]) << (9 * 1)) +
            (((uintptr_t)path[3]) << (9 * 0));

    uintptr_t indices[4];

    indices[0] = (page_index >> (9 * 3)) & (PT0_ENTRIES-1);
    indices[1] = (page_index >> (9 * 2)) & (PT1_ENTRIES-1);
    indices[2] = (page_index >> (9 * 1)) & (PT2_ENTRIES-1);
    indices[3] = (page_index >> (9 * 0)) & (PT3_ENTRIES-1);

    pte[0] = PT0_PTR(base) + indices[0];
    pte[1] = PT1_PTR(base) + indices[1];
    pte[2] = PT2_PTR(base) + indices[2];
    pte[3] = PT3_PTR(base) + indices[3];
}

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
                        (((linaddr_t)first_available) << PAGE_SIZE_BIT);

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
    if (address >= PT3_PTR(PT_BASEADDR) + PT3_ENTRIES - 64 &&
            address < PT3_PTR(PT_BASEADDR) + PT3_ENTRIES) {
        // Address refers to a page table entry
        bit = address - (PT3_PTR(PT_BASEADDR) + PT3_ENTRIES - 64);
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

static physaddr_t init_take_page(int low)
{
    if (likely(usable_mem_ranges == 0))
        return mmu_alloc_phys(low);

    assert(usable_mem_ranges > 0);

    physmem_range_t *last_range = mem_ranges + usable_mem_ranges - 1;
    physaddr_t addr = last_range->base + last_range->size - PAGE_SIZE;

    // Take a page off the size of the range
    last_range->size -= PAGE_SIZE;

    // If range exhausted, switch to next range
    if (last_range->size == 0)
        --usable_mem_ranges;

#if DEBUG_PHYS_ALLOC
    //printdbg("Took early page @ %lx\n", addr);
#endif

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
    linaddr_t linaddr;

    if ((linaddr_t)aliasing_pte >= PT_BASEADDR) {
        uintptr_t pt3_index = aliasing_pte - PT3_PTR(PT_BASEADDR);
        linaddr = (pt3_index << PAGE_SIZE_BIT);
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
        physaddr_t root,
        pte_t *aliasing_pte,
        linaddr_t linear_addr,
        physaddr_t phys_addr,
        physaddr_t *pt_physaddr,
        pte_t page_flags)
{
    // Path to page table entry as page indices
    unsigned path[4];
    path_from_addr(path, linear_addr);

    unsigned levels = (phys_addr == ~0UL) ? 4 : 3;

    pte_t volatile *iter;
    pte_t old_pte;

    // Map aliasing page to point to new page
    iter = mm_map_aliasing_pte(aliasing_pte, root);

    if (pt_physaddr)
        pt_physaddr[0] = root;

    physaddr_t addr;
    for (unsigned level = 0; level < levels; ++level) {
        addr = iter[path[level]] & PTE_ADDR;

        if (!addr) {
            // Allocate a new page table
            addr = init_take_page(0);

            assert(addr != 0);

            // Atomically update current page table to point to new page
            old_pte = atomic_cmpxchg(iter + path[level],
                                     0, addr | page_flags | PTE_PRESENT);
            if (old_pte == 0) {
#if DEBUG_PHYS_ALLOC
                printdbg("Assigned page table for level=%u"
                         " %3u/%3u/%3u/%3u %12lx @ %13lx\n",
                         level, path[0], path[1], path[2], path[3],
                        linear_addr, addr);
#endif

                // Map aliasing page to point to new page
                iter = init_map_aliasing_pte(aliasing_pte, addr);

                // Clear new page
                aligned16_memset((void*)iter, 0, PAGE_SIZE);
            } else {
#if DEBUG_PHYS_ALLOC
                printdbg("Racing thread already assigned page table for level=%u"
                         " %3u/%3u/%3u/%3u %12lx @ %13lx\n",
                         level, path[0], path[1], path[2], path[3],
                        linear_addr, addr);
#endif

                // Return page to pool
                mmu_free_phys(addr);

                addr = old_pte & PTE_ADDR;

                // Descend into next page table
                iter = init_map_aliasing_pte(aliasing_pte, addr);
            }
        } else {
            // Descend into next page table
            iter = init_map_aliasing_pte(aliasing_pte, addr);
        }

        if (pt_physaddr)
            pt_physaddr[level+1] = addr;
    }

    if (levels == 3 && !(iter[path[3]] & PTE_PRESENT)) {
        old_pte = atomic_cmpxchg(iter + path[3],
                0, (phys_addr & PTE_ADDR) | page_flags);

        assert(old_pte == 0);

#if DEBUG_PHYS_ALLOC
        printdbg("Assigned page table for level=%u"
                 " %3u/%3u/%3u/%3u %12lx @ %13lx\n",
                 3, path[0], path[1], path[2], path[3],
                linear_addr, phys_addr);
#endif

        cpu_invalidate_page(linear_addr);
    }
}

static void mmu_map_page(
        linaddr_t addr,
        physaddr_t physaddr, pte_t flags)
{
    // Read root physical address from page tables
    physaddr_t root = cpu_get_page_directory();

    // Get a free aliasing PTE
    pte_t *aliasing_pte = take_apte(~0UL);

    physaddr_t pt_physaddr[4];
    init_create_pt(root, aliasing_pte,
                   addr, physaddr,
                   pt_physaddr,
                   flags);

    unsigned path[4];
    path_from_addr(path, addr);

    pte_t *pte_linaddr[4];
    pte_from_path(pte_linaddr, path);

    // Map the page tables for the region
    for (unsigned i = 0; i < 4; ++i) {
        init_create_pt(root, aliasing_pte,
                       (linaddr_t)pte_linaddr[i],
                       pt_physaddr[i], 0,
                       PTE_PRESENT | PTE_WRITABLE);
    }

    release_apte(aliasing_pte);
}

static int ptes_present(pte_t **ptes)
{
    int present_mask;

    present_mask = (*ptes[0] & PTE_PRESENT);
    present_mask |= (present_mask == 1 &&
                     (*ptes[1] & PTE_PRESENT)) << 1;
    present_mask |= (present_mask == 3 &&
                     (*ptes[2] & PTE_PRESENT)) << 2;
    present_mask |= (present_mask == 7 &&
                     (*ptes[3] & PTE_PRESENT)) << 3;

    return present_mask;
}

static int path_present(unsigned *path, pte_t **ptes)
{
    pte_from_path(ptes, path);
    return ptes_present(ptes);
}

static int addr_present(uintptr_t addr, unsigned *path, pte_t **ptes)
{
    path_from_addr(path, addr);
    return path_present(path, ptes);
}

// TLB shootdown IPI
static void *mmu_tlb_shootdown(int intr, void *ctx)
{
    assert(intr == INTR_TLB_SHOOTDOWN);
    //printdbg("Received TLB shootdown\n");
    thread_set_cpu_mmu_seq(mmu_seq);
    cpu_flush_tlb();
    apic_eoi(intr);
    return ctx;
}

static void mmu_send_tlb_shootdown(void)
{
    apic_send_ipi(-1, INTR_TLB_SHOOTDOWN);
}

static void *mmu_lazy_tlb_shootdown(void *ctx)
{
    thread_set_cpu_mmu_seq(mmu_seq);
    cpu_flush_tlb();

    // Restart instruction
    return ctx;
}

// Page fault
static void *mmu_page_fault_handler(int intr, void *ctx)
{
    (void)intr;
    assert(intr == INTR_EX_PAGE);

    atomic_inc_uint64(&page_fault_count);

    isr_context_t *ic = ctx;

    uintptr_t fault_addr = cpu_get_fault_address();

#if DEBUG_PAGE_FAULT
    printdbg("Page fault at %lx\n", fault_addr);
#endif

    unsigned path[4];
    pte_t *ptes[4];

    int present_mask = addr_present(fault_addr, path, ptes);

    // Check for lazy TLB shootdown
    if (present_mask == 0x0F && thread_get_cpu_mmu_seq() != mmu_seq)
        return mmu_lazy_tlb_shootdown(ctx);

    pte_t pte = *ptes[3];

    // If the page table exists
    if (present_mask == 0x07) {
        // If it is lazy allocated
        if ((pte & PTE_ADDR) == PTE_ADDR) {
            // Allocate a page
            physaddr_t page = mmu_alloc_phys(0);

            assert(page != 0);

#if DEBUG_PAGE_FAULT
            printdbg("Assigning %lx with page %lx\n",
                     fault_addr, page);
#endif

            pte_t page_flags;

            if (ic->gpr->info.error_code & CTX_ERRCODE_PF_W)
                page_flags = PTE_PRESENT | PTE_ACCESSED | PTE_DIRTY;
            else
                page_flags = PTE_PRESENT | PTE_ACCESSED;

            // Update PTE and restart instruction
            if (atomic_cmpxchg(ptes[3], pte,
                               (pte & ~PTE_ADDR) |
                               (page & PTE_ADDR) |
                               page_flags) != pte) {
                // Another thread beat us to it
                printdbg("Racing thread already assigned page"
                         " for %lx, freeing page %lx\n",
                         fault_addr, page);

                mmu_free_phys(page);
                cpu_invalidate_page(fault_addr);
            }

            return ctx;
        } else {
            assert(!"Invalid page fault");
        }
    } else if (present_mask != 0x0F) {
        assert(!"Invalid page fault path");
    } else {
        assert(!"Unexpected page fault");
    }

    return 0;
}

#if DEBUG_ADDR_ALLOC
static int dump_addr_node(rbtree_t *tree,
                           rbtree_kvp_t *kvp,
                           void *p)
{
    (void)tree;
    (void)p;
    printdbg("key=%12lx val=%12lx\n",
             kvp->key, kvp->val);
    return 0;
}

static void dump_addr_tree(rbtree_t *tree, char const *name)
{
    printdbg("%s dump\n", name);
    rbtree_walk(tree, dump_addr_node, 0);
    printdbg("---------------------------------\n");
}
#endif

//
// Initialization

void mmu_init(int ap)
{
    if (ap) {
        cpu_set_page_directory(root_physaddr);
        return;
    }

    // Hook IPI for TLB shootdown
    intr_hook(INTR_TLB_SHOOTDOWN, mmu_tlb_shootdown);

    usable_mem_ranges = mmu_fixup_mem_map(phys_mem_map);

    if (usable_mem_ranges > countof(mem_ranges)) {
        printdbg("Physical memory is incredibly fragmented!\n");
        usable_mem_ranges = countof(mem_ranges);
    }

    for (physmem_range_t *ranges_in = phys_mem_map, *ranges_out = mem_ranges;
         ranges_in < phys_mem_map + phys_mem_map_count;
         ++ranges_in) {
        if (ranges_in->type == PHYSMEM_TYPE_NORMAL)
            *ranges_out++ = *ranges_in;

        // Cap
        if (ranges_out >= mem_ranges + usable_mem_ranges)
            break;
    }

    size_t usable_pages = 0;
    physaddr_t highest_usable = 0;
    for (physmem_range_t *mem = mem_ranges;
         mem < mem_ranges + usable_mem_ranges; ++mem) {
        printdbg("Memory: addr=%lx size=%lx type=%x\n",
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
    printdbg("Usable pages = %lu (%luMB) range_pages=%ld\n", usable_pages,
           usable_pages >> (20 - PAGE_SIZE_BIT), highest_usable);

    //
    // Alias the existing page tables into the appropriate addresses

    pte_t *aliasing_pte = init_find_aliasing_pte();

    // Create the new root

    // Get a page
    root_physaddr = init_take_page(0);

    assert(root_physaddr != 0);

    // Map page
    pte_t *root = init_map_aliasing_pte(aliasing_pte, root_physaddr);

    // Clear page
    aligned16_memset(root, 0, PAGE_SIZE);

    init_create_pt(root_physaddr, aliasing_pte,
                   (linaddr_t)PT0_PTR(PT_BASEADDR),
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
                            ((uintptr_t)path[0] << (9 * 3 + 12)) |
                            ((uintptr_t)path[1] << (9 * 2 + 12)) |
                            ((uintptr_t)path[2] << (9 * 1 + 12)) |
                            ((uintptr_t)path[3] << (9 * 0 + 12)),
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
    *PT3_PTR(PT_BASEADDR) = 0;
    cpu_invalidate_page(0);

    phys_alloc_count = highest_usable;
    phys_alloc = mmap(0, phys_alloc_count * sizeof(*phys_alloc),
         PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    printdbg("Building physical memory free list\n");

    memset(phys_alloc, 0, phys_alloc_count * sizeof(*phys_alloc));

    // Put all of the remaining physical memory into the free lists
    uintptr_t free_count = 0;
    physaddr_t top_of_kernel = ___top_physaddr;
    for (; ; ++free_count) {
        physaddr_t addr = init_take_page(0);

        assert(addr != 0);

        if (addr > top_of_kernel &&
                addr < 0x8000000000000000UL) {
            uint64_t volatile *chain = phys_next_free +
                    (addr < 0x100000000UL);
            addr -= 0x100000U;
            addr >>= PAGE_SIZE_BIT;
            assert(addr < phys_alloc_count);
            phys_alloc[addr] = *chain;
            *chain = addr;

            ++free_page_count;
        } else {
            //for (unsigned i = 0; i < phys_alloc_count; ++i)
            //    printdbg("[%x]: %x\n", i, phys_alloc[i]);
            break;
        }
    }

    release_apte(aliasing_pte);

    // Start using physical memory allocator
    usable_mem_ranges = 0;

    printdbg("%lu pages free (%luMB)\n",
           free_count,
           free_count >> (20 - PAGE_SIZE_BIT));

    intr_hook(INTR_EX_PAGE, mmu_page_fault_handler);

    callout_call('M');

    // Allocate guard page
    take_linear(PAGE_SIZE);

    mutex_init(&free_addr_lock);

    free_addr_by_addr = rbtree_create(take_linear_cmp_key, 0, 1 << 20);
    free_addr_by_size = rbtree_create(take_linear_cmp_both, 0, 1 << 20);

    rbtree_insert(free_addr_by_size,
                  PT_BASEADDR - linear_allocator,
                  linear_allocator);
    rbtree_insert(free_addr_by_addr,
                  linear_allocator,
                  PT_BASEADDR - linear_allocator);
}

static size_t round_up(size_t n)
{
    return (n + PAGE_MASK) & -PAGE_SIZE;
}

static size_t round_down(size_t n)
{
    return n & ~PAGE_MASK;
}

//
// Linear address allocator

static int take_linear_cmp_key(
        rbtree_kvp_t const *lhs,
        rbtree_kvp_t const *rhs,
        void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            0;
}

static int take_linear_cmp_both(
        rbtree_kvp_t const *lhs,
        rbtree_kvp_t const *rhs,
        void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            lhs->val < rhs->val ? -1 :
            rhs->val < lhs->val ? 1 :
            0;
}

static void sanity_check_by_size(rbtree_t *tree)
{
    static int call = 0;
    ++call;

    rbtree_kvp_t prev = { 0, 0 };
    rbtree_kvp_t curr = { 0, 0 };

    for (rbtree_iter_t it = rbtree_first(tree, 0);
         it;
         it = rbtree_next(tree, it)) {
        curr = rbtree_item(tree, it);
        assert(prev.val + prev.key != curr.val);
        prev = curr;
    }
}

static void sanity_check_by_addr(rbtree_t *tree)
{
    rbtree_kvp_t prev = { 0, 0 };
    rbtree_kvp_t curr = { 0, 0 };

    for (rbtree_iter_t it = rbtree_first(tree, 0);
         it;
         it = rbtree_next(tree, it)) {
        curr = rbtree_item(tree, it);
        assert(prev.key + prev.val != curr.key);
        prev = curr;
    }
}

static linaddr_t take_linear(size_t size)
{
    // Round up to a multiple of the page size
    size = round_up(size);

    linaddr_t addr;

    if (free_addr_by_addr && free_addr_by_size) {
        mutex_lock(&free_addr_lock);

#if DEBUG_ADDR_ALLOC
        printdbg("---- Alloc %lx\n", size);
        dump_addr_tree(free_addr_by_addr, "Addr map by addr (before alloc)");
        //dump_addr_tree(free_addr_by_size, "Addr map by size (before alloc)");
#endif

        // Find the lowest address item that is big enough
        rbtree_iter_t place = rbtree_lower_bound(
                    free_addr_by_size, size, 0);

        rbtree_kvp_t by_size = rbtree_item(free_addr_by_size, place);

        if (by_size.key < size) {
            place = rbtree_next(free_addr_by_size, place);
            by_size = rbtree_item(free_addr_by_size, place);
        }

        rbtree_delete_at(free_addr_by_size, place);

        // Delete corresponding entry by address
        rbtree_delete(free_addr_by_addr, by_size.val, by_size.key);

        if (by_size.key > size) {
            // Insert remainder by size
            rbtree_insert(free_addr_by_size,
                          by_size.key - size,
                          by_size.val + size);

            // Insert remainder by address
            rbtree_insert(free_addr_by_addr,
                          by_size.val + size,
                          by_size.key - size);
        }

        addr = by_size.val;

#if DEBUG_ADDR_ALLOC
        dump_addr_tree(free_addr_by_addr, "Addr map by addr (after alloc)");
        //dump_addr_tree(free_addr_by_size, "Addr map by size (after alloc)");

        printdbg("%lx ----\n", addr);
#endif

        mutex_unlock(&free_addr_lock);

        sanity_check_by_size(free_addr_by_size);
        sanity_check_by_addr(free_addr_by_addr);
    } else {
        addr = atomic_xadd_uint64(&linear_allocator, size);
    }

    return addr;
}

static void release_linear(linaddr_t addr, size_t size)
{
    // Round address down to page boundary
    size_t misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;

    // Round size up to multiple of page size
    size = round_up(size);

    linaddr_t end = addr + size;

    mutex_lock(&free_addr_lock);

#if DEBUG_ADDR_ALLOC
    printdbg("---- Free %lx @ %lx\n", size, addr);
    dump_addr_tree(free_addr_by_addr, "Addr map by addr (before free)");
    //dump_addr_tree(free_addr_by_size, "Addr map by size (before free)");
#endif

    // Find the nearest free block before the freed range
    rbtree_iter_t pred_it = rbtree_lower_bound(
                free_addr_by_addr, addr, 0);

    // Find the nearest free block after the freed range
    rbtree_iter_t succ_it = rbtree_lower_bound(
                free_addr_by_addr, end, ~0UL);

    rbtree_kvp_t pred = rbtree_item(free_addr_by_addr, pred_it);
    rbtree_kvp_t succ = rbtree_item(free_addr_by_addr, succ_it);

    int coalesce_pred = ((pred.key + pred.val) == addr);
    int coalesce_succ = (succ.key == end);

    if (coalesce_pred) {
        addr -= pred.val;
        size += pred.val;
        rbtree_delete_at(free_addr_by_addr, pred_it);

        rbtree_delete(free_addr_by_size, pred.val, pred.key);
    }

    if (coalesce_succ) {
        size += succ.val;
        rbtree_delete_at(free_addr_by_addr, succ_it);

        rbtree_delete(free_addr_by_size, succ.val, succ.key);
    }

    rbtree_insert(free_addr_by_size, size, addr);
    rbtree_insert(free_addr_by_addr, addr, size);

#if DEBUG_ADDR_ALLOC
    dump_addr_tree(free_addr_by_addr, "Addr map by addr (after free)");
    //dump_addr_tree(free_addr_by_size, "Addr map by size (after free)");
#endif

    mutex_unlock(&free_addr_lock);
}

static int mmu_have_nx(void)
{
    // 0 == unknown, 1 == supported, -1 == not supported
    static int supported;
    if (supported != 0)
        return supported > 0;

    cpuid_t info;
    supported = (cpuid(&info, 0x80000001, 0) &&
                 !!(info.edx & (1<<20)))
            ? 1
            : -1;

    return supported > 0;
}

#if 0
void map_page_tables(linaddr_t addr_st, size_t len)
{
    // Inclusive end
    linaddr_t addr_en = addr_st + len - 1;

    //
    // Start and end paths and ptes for mapped region

    unsigned path_st[4];
    pte_t *pte_st[4];

    unsigned path_en[4];
    pte_t *pte_en[4];

    path_from_addr(path_st, addr_st);
    path_from_addr(path_en, addr_en);

    pte_from_path(pte_st, path_st);
    pte_from_path(pte_en, path_en);

    //
    // Start and end paths and ptes for page tables for region

    unsigned pte_path_st[4];
    pte_t *pte_pte_st[4];

    unsigned pte_path_en[4];
    pte_t *pte_pte_en[4];

    for (int pt_pt_level = 0; pt_pt_level < 4; ++pt_pt_level) {
        path_from_addr(pte_path_st, (linaddr_t)pte_st[pt_pt_level]);
        path_from_addr(pte_path_en, (linaddr_t)pte_en[pt_pt_level]);

        pte_from_path(pte_pte_st, pte_path_st);
        pte_from_path(pte_pte_en, pte_path_en);

        for (pte_t *slot = pte_pte_st[pt_pt_level];
             slot <= pte_pte_en[pt_pt_level]; ++slot) {

            for (int pt_level; pt_level < 4; ++pt_level) {
                if (*slot == 0) {
                    physaddr_t page = init_take_page(0);
                    *slot = page | PTE_PRESENT | PTE_WRITABLE;
                }
            }
        }
    }
}
#endif

//
// Public API

int mpresent(uintptr_t addr)
{
    unsigned path[4];
    pte_t *ptes[4];
    return addr_present(addr, path, ptes) == 0x0F;
}

void *mmap(void *addr, size_t len,
           int prot, int flags,
           int fd, off_t offset)
{
    // Bomb out on unsupported stuff, for now
    if (fd != -1 || offset != 0 || !len)
        return 0;

#if DEBUG_PAGE_TABLES
    printdbg("Mapping len=%zx prot=%x flags=%x addr=%lx\n",
             len, prot, flags, (uintptr_t)addr);
#endif

    pte_t page_flags = 0;

    if (flags & (MAP_STACK | MAP_32BIT))
        flags |= MAP_POPULATE;

    if (flags & MAP_PHYSICAL)
        page_flags |= PTE_PCD | PTE_PWT |
                PTE_EX_PHYSICAL | PTE_PRESENT;

    if (flags & MAP_NOCACHE)
        page_flags |= PTE_PCD;

    if (flags & MAP_WRITETHRU)
        page_flags |= PTE_PWT;

    if (flags & MAP_POPULATE)
        page_flags |= PTE_PRESENT;

    if (prot & PROT_WRITE)
        page_flags |= PTE_WRITABLE;

    if (!(prot & PROT_EXEC) && mmu_have_nx())
        page_flags |= PTE_NX;

    uintptr_t misalignment = 0;

    if (unlikely(flags & MAP_PHYSICAL)) {
        misalignment = (uintptr_t)addr & PAGE_MASK;
        len += misalignment;
    }

    linaddr_t linear_addr = take_linear(len);

    assert(linear_addr > 0x100000);

    for (size_t ofs = 0; ofs < len; ofs += PAGE_SIZE)
    {
        if (likely(!(flags & MAP_PHYSICAL))) {
            // Allocate normal memory

            // Not present pages with max physaddr are demand committed
            physaddr_t page = PTE_ADDR;

            // If populating, assign physical memory immediately
            if (flags & MAP_POPULATE) {
                page = init_take_page(!!(flags & MAP_32BIT));
                assert(page != 0);
            }

            mmu_map_page(linear_addr + ofs, page, page_flags);
        } else {
            // addr is a physical address, caller uses
            // returned linear address to access it
            mmu_map_page(linear_addr + ofs,
                         (((physaddr_t)addr) + ofs) & PTE_ADDR,
                         page_flags);
        }
    }

    atomic_inc_uint64(&mmu_seq);
    //mmu_send_tlb_shootdown();

    assert(linear_addr > 0x100000);

    return (void*)(linear_addr + misalignment);
}

void *mremap(
        void *old_address,
        size_t old_size,
        size_t new_size,
        int flags,
        ... /* void *__new_address */)
{
    void *new_address = 0;
    (void)new_address;

    if (!(flags & MREMAP_FIXED)) {
        va_list ap;
        va_start(ap, flags);
        new_address = va_arg(ap, void*);
        va_end(ap);
    }

    old_size = round_up(old_size);
    new_size = round_up(new_size);

    // Convert pointer to address
    uintptr_t old_st = (uintptr_t)old_address;

    if (new_size < old_size) {
        //
        // Got smaller

        uintptr_t freed_size = old_size - new_size;

        // Release space at the end
        munmap((void*)(old_st + new_size), freed_size);
    } else if (new_size > old_size) {
        //
        // Got bigger

        linaddr_t new_linear;

        new_linear = take_linear(new_size);

        unsigned path[4];
        path_from_addr(path, new_linear);

        pte_t *new_pte[4];
        pte_from_path(new_pte, path);

        pte_t *old_pte[4];
        path_from_addr(path, old_st);
        pte_from_path(old_pte, path);

        // FIXME: Move PTEs...
    }

    return 0;
}

int munmap(void *addr, size_t size)
{
    linaddr_t a = (linaddr_t)addr;

    uintptr_t misalignment = 0;

    misalignment = a & PAGE_MASK;
    a -= misalignment;
    size += misalignment;
    size = round_up(size);

    unsigned path[4];
    path_from_addr(path, a);

    pte_t *pteptr[4];

    for (size_t ofs = 0; ofs < size; ofs += PAGE_SIZE)
    {
        int present_mask = path_present(path, pteptr);
        if ((present_mask & 0x07) == 0x07) {
            pte_t pte = atomic_xchg(pteptr[3], 0);

            if (!(pte & PTE_EX_PHYSICAL)) {
                physaddr_t physaddr = pte & PTE_ADDR;

                if (physaddr && (physaddr != PTE_ADDR))
                    mmu_free_phys(physaddr);
            }

            if (present_mask == 0x0F)
                cpu_invalidate_page(a);
        }

        a += PAGE_SIZE;
        path_inc(path);
    }

    mmu_send_tlb_shootdown();

    release_linear((linaddr_t)addr - misalignment, size);

    return 0;
}

uintptr_t mphysaddr(void *addr)
{
    linaddr_t linaddr = (linaddr_t)addr;

    uintptr_t misalignment = linaddr & PAGE_MASK;

    unsigned path[4];
    path_from_addr(path, linaddr);

    pte_t *pte[4];
    pte_from_path(pte, path);

    return ((*pte[3]) & PTE_ADDR) + misalignment;
}

static inline int mphysranges_enum(
        void *addr, size_t size,
        int (*callback)(mmphysrange_t, void*),
        void *context)
{
    linaddr_t linaddr = (linaddr_t)addr;
    size_t remaining_size = size;
    physaddr_t page_end;
    mmphysrange_t range;
    int result;

    do {
        // Get physical address for this linear address
        range.physaddr = mphysaddr((void*)linaddr);

        // Calculate physical address of the end of the page
        page_end = round_down(range.physaddr + PAGE_SIZE);

        // Calculate size of data until end of page
        range.size = page_end - range.physaddr;

        // Cap at requested maximum
        if (unlikely(range.size > remaining_size))
            range.size = remaining_size;

        // Advance the current linear address
        // Pass this range to the callback
        // Remember the callback return value
        // Loop again if the callback did not return 0 and
        // the remaining size is not equal to zero
    } while (linaddr += range.size,
             ((result = callback(range, context)) &&
             (remaining_size -= range.size)));

    return result;
}

typedef struct mphysranges_state_t {
    mmphysrange_t *range;
    size_t ranges_count;
    size_t count;
    size_t max_size;
    physaddr_t last_end;
    mmphysrange_t cur_range;
} mphysranges_state_t;

static inline int mphysranges_callback(mmphysrange_t range, void *context)
{
    mphysranges_state_t *state = context;

    int contiguous = state->count > 0 &&
            (state->range->physaddr + state->range->size == range.physaddr);

    if (state->count == 0) {
        // Initial range
        *state->range = range;
        ++state->count;
    } else if (state->range->size < state->max_size && contiguous) {
        // Extend the range
        state->range->size += range.size;
    } else if (state->count < state->ranges_count && range.size > 0) {
        *++state->range = range;
        ++state->count;
    } else {
        return 0;
    }

    return 1;
}

size_t mphysranges(mmphysrange_t *ranges,
                   size_t ranges_count,
                   void *addr, size_t size,
                   size_t max_size)
{
    mphysranges_state_t state;

    // Request data
    state.ranges_count = ranges_count;
    state.max_size = max_size;

    // Result data
    state.range = ranges;
    state.count = 0;
    state.last_end = 0;
    state.cur_range.physaddr = 0;
    state.cur_range.size = 0;

    mphysranges_enum(addr, size, mphysranges_callback, &state);

    if (state.count < ranges_count) {
        // Flush last region
        state.cur_range.physaddr = ~0UL;
        state.cur_range.size = 0;
        mphysranges_callback(state.cur_range, &state);
    }

    return state.count;
}

