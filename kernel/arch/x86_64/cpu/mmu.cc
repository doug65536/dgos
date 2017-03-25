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
#include "bsearch.h"
#include "process.h"
#include "cpu_broadcast.h"

#define DEBUG_CREATE_PT         0
#define DEBUG_ADDR_ALLOC        0
#define DEBUG_PHYS_ALLOC        0
#define DEBUG_PAGE_TABLES       0
#define DEBUG_PAGE_FAULT        0
#define DEBUG_LINEAR_SANITY     0

#define PROFILE_PHYS_ALLOC      0
#if PROFILE_PHYS_ALLOC
#define PROFILE_PHYS_ALLOC_ONLY(p) p
#else
#define PROFILE_PHYS_ALLOC_ONLY(p)
#endif

#define PROFILE_LINEAR_ALLOC    0
#if PROFILE_LINEAR_ALLOC
#define PROFILE_LINEAR_ALLOC_ONLY(p) p
#else
#define PROFILE_LINEAR_ALLOC_ONLY(p)
#endif

#define PROFILE_MMAP            0
#if PROFILE_MMAP
#define PROFILE_MMAP_ONLY(p) p
#else
#define PROFILE_MMAP_ONLY(p)
#endif

#define PROFILE_SLOWPATH        0
#if PROFILE_SLOWPATH
#define PROFILE_SLOWPATH_ONLY(p) p
#else
#define PROFILE_SLOWPATH_ONLY(p)
#endif

// Intel manual, page 2786

// The entries of the 4 levels of page tables are named:
//  PML4E (maps 512 512GB regions)
//  PDPTE (maps 512 1GB regions)
//  PDE   (maps 512 2MB regions)
//  PTE   (maps 512 4KB regions)

// 4KB pages
#define PAGE_SIZE_BIT       12
#ifndef PAGE_SIZE
#define PAGE_SIZE           (1UL << PAGE_SIZE_BIT)
#endif
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
#define PTE_PDEPAT_BIT      12  // only PDPTE and PDE
#define PTE_PTEPAT_BIT      7  // only PTE
#define PTE_ADDR_BIT        12
#define PTE_PK_BIT          59
#define PTE_NX_BIT          63

// Always ignored areas 11:9 58:52
#define PTE_AVAIL1_BIT      9
#define PTE_AVAIL2_BIT      52

// Assigned usage of available bits
#define PTE_EX_PHYSICAL_BIT (PTE_AVAIL1_BIT+0)
#define PTE_EX_LOCKED_BIT   (PTE_AVAIL1_BIT+1)
#define PTE_EX_DEVICE_BIT   (PTE_AVAIL1_BIT+2)

// Size of multi-bit fields
#define PTE_PK_BITS         4
#define PTE_ADDR_BITS       40
#define PTE_AVAIL1_BITS     3
#define PTE_AVAIL2_BITS     7

// Size of physical address including low bits
#define PTE_FULL_ADDR_BITS  (PTE_ADDR_BITS+PAGE_SIZE_BIT)

// Bitmask for multi-bit field values
// Aligned to bit 0
#define PTE_PK_MASK         ((1UL << PTE_PK_BITS) - 1U)
#define PTE_ADDR_MASK       ((1UL << PTE_ADDR_BITS) - 1U)
#define PTE_AVAIL1_MASK     ((1UL << PTE_AVAIL1_BITS) - 1U)
#define PTE_AVAIL2_MASK     ((1UL << PTE_AVAIL2_BITS) - 1U)
#define PTE_FULL_ADDR_MASK  ((1UL << PTE_FULL_ADDR_BITS) - 1U)

// Values of bits
#define PTE_PRESENT         (1UL << PTE_PRESENT_BIT)
#define PTE_WRITABLE        (1UL << PTE_WRITABLE_BIT)
#define PTE_USER            (1UL << PTE_USER_BIT)
#define PTE_PWT             (1UL << PTE_PWT_BIT)
#define PTE_PCD             (1UL << PTE_PCD_BIT)
#define PTE_ACCESSED        (1UL << PTE_ACCESSED_BIT)
#define PTE_DIRTY           (1UL << PTE_DIRTY_BIT)
#define PTE_PAGESIZE        (1UL << PTE_PAGESIZE_BIT)
#define PTE_GLOBAL          (1UL << PTE_GLOBAL_BIT)
#define PTE_PDEPAT          (1UL << PTE_PDEPAT_BIT)
#define PTE_NX              (1UL << PTE_NX_BIT)
#define PTE_PDEPAT          (1UL << PTE_PDEPAT_BIT)
#define PTE_PTEPAT          (1UL << PTE_PTEPAT_BIT)

// Multi-bit field masks, in place
#define PTE_ADDR            (PTE_ADDR_MASK << PTE_ADDR_BIT)
#define PTE_PK              (PTE_PK_MASK << PTE_PK_BIT)
#define PTE_AVAIL1          (PTE_AVAIL1_MASK << PTE_AVAIL1_BIT)
#define PTE_AVAIL2          (PTE_AVAIL2_MASK << PTE_AVAIL2_BIT)

// Assigned usage of available PTE bits
#define PTE_EX_PHYSICAL     (1UL << PTE_EX_PHYSICAL_BIT)
#define PTE_EX_LOCKED       (1UL << PTE_EX_LOCKED_BIT)
#define PTE_EX_DEVICE       (1UL << PTE_EX_DEVICE_BIT)

// PAT configuration
#define PAT_IDX_WB  0
#define PAT_IDX_WT  1
#define PAT_IDX_UCW 2
#define PAT_IDX_UC  3
#define PAT_IDX_WC  4
#define PAT_IDX_WP  5

#define PAT_CFG \
    (MSR_IA32_PAT_n(PAT_IDX_WB, MSR_IA32_PAT_WB) | \
    MSR_IA32_PAT_n(PAT_IDX_WT, MSR_IA32_PAT_WT) | \
    MSR_IA32_PAT_n(PAT_IDX_UCW, MSR_IA32_PAT_UCW) | \
    MSR_IA32_PAT_n(PAT_IDX_UC, MSR_IA32_PAT_UC) | \
    MSR_IA32_PAT_n(PAT_IDX_WC, MSR_IA32_PAT_WC) | \
    MSR_IA32_PAT_n(PAT_IDX_WP, MSR_IA32_PAT_WP))

#define PTE_PDEPAT_n(idx) \
    ((PTE_PDEPAT & -!!(idx & 4)) | \
    (PTE_PCD & -!!(idx & 2)) | \
    (PTE_PWT & -!!(idx & 1)))

#define PTE_PTEPAT_n(idx) \
    ((PTE_PTEPAT & -!!(idx & 4)) | \
    (PTE_PCD & -!!(idx & 2)) | \
    (PTE_PWT & -!!(idx & 1)))

// Page table entries don't have a structure, they
// are a bunch of bitfields. Use uint64_t and the
// constants above
typedef uintptr_t pte_t;

typedef uintptr_t physaddr_t;
typedef uintptr_t linaddr_t;

//
// Recursive page table mapping

// Recursive mapping index calculation
#define PT_ENTRY(i0,i1,i2,i3) \
    ((((((((i0)<<9)+(i1))<<9)+(i2))<<9)+(i3))<<9)

// The number of pte_t entries at each level
#define PT3_ENTRIES     (0x1000000000UL)
#define PT2_ENTRIES     (0x8000000UL)
#define PT1_ENTRIES     (0x40000UL)
#define PT0_ENTRIES     (0x200UL)

#define PT_RECURSE      (256UL)

// Indices of start of page tables for each level
#define PT3_INDEX       (PT_ENTRY(PT_RECURSE,0,0,0))
#define PT2_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,0,0))
#define PT1_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,PT_RECURSE,0))
#define PT0_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,PT_RECURSE,PT_RECURSE))

// Canonicalize the given address
#define CANONICALIZE(n) (((uintptr_t)(-(((intptr_t)(n))>>47))<<47)|(n))

#define PT3_ADDR        (CANONICALIZE(PT3_INDEX*sizeof(pte_t)))
#define PT2_ADDR        (CANONICALIZE(PT2_INDEX*sizeof(pte_t)))
#define PT1_ADDR        (CANONICALIZE(PT1_INDEX*sizeof(pte_t)))
#define PT0_ADDR        (CANONICALIZE(PT0_INDEX*sizeof(pte_t)))

#define PT3_PTR         ((pte_t*)PT3_ADDR)
#define PT2_PTR         ((pte_t*)PT2_ADDR)
#define PT1_PTR         ((pte_t*)PT1_ADDR)
#define PT0_PTR         ((pte_t*)PT0_ADDR)

#define PT_BASEADDR     (PT0_ADDR)
#define PT_MAX_ADDR     (PT0_ADDR + (512UL << 30))

//
// Device mapping

// Device registration for memory mapped device
typedef struct mmap_device_mapping_t {
    void *base_addr;
    uint64_t len;
    mm_dev_mapping_callback_t callback;
    void *context;
    mutex_t lock;
    condition_var_t done_cond;
    int64_t active_read;
} mmap_device_mapping_t;

static int mm_dev_map_search(void const *v, void const *k, void *s);

#define MM_MAX_DEVICE_MAPPINGS  16
static mmap_device_mapping_t mm_dev_mappings[MM_MAX_DEVICE_MAPPINGS];
static unsigned mm_dev_mapping_count;
static spinlock_t mm_dev_mapping_lock;

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
static physmem_range_t mem_ranges[64];
static size_t usable_mem_ranges;

static physaddr_t root_physaddr;
static pte_t const * master_pagedir;
static pte_t *current_pagedir;

static uint32_t *phys_alloc;
static unsigned phys_alloc_count;

extern char ___init_brk[];
extern uintptr_t ___top_physaddr;
static linaddr_t near_base = (linaddr_t)___init_brk;
static linaddr_t volatile linear_base = PT_MAX_ADDR;

// Maintain two free page chains,
// because some hardware requires 32-bit memory addresses
// [0] is the chain >= 4GB
// [1] is the chain < 4GB
static uint64_t volatile phys_next_free[2];

// Incremented every time the page tables are changed
// Used to detect lazy TLB shootdown
static uint64_t volatile mmu_seq;

static uint64_t volatile shootdown_pending;

static int contiguous_allocator_cmp_key(
        typename rbtree_t<>::kvp_t const *lhs,
        typename rbtree_t<>::kvp_t const *rhs,
        void *p);

static int contiguous_allocator_cmp_both(
        typename rbtree_t<>::kvp_t const *lhs,
        typename rbtree_t<>::kvp_t const *rhs,
        void *p);

static uint64_t volatile page_fault_count;

static int64_t volatile free_page_count;

//
// Contiguous allocator

typedef struct contiguous_allocator_t {
    mutex_t free_addr_lock;
    rbtree_t<> *free_addr_by_size;
    rbtree_t<> *free_addr_by_addr;
} contiguous_allocator_t;

static void contiguous_allocator_create(contiguous_allocator_t *allocator,
        linaddr_t *addr, size_t size, size_t capacity);

static uintptr_t alloc_linear(
        contiguous_allocator_t *allocator,
        size_t size);
static void release_linear(contiguous_allocator_t *allocator,
        uintptr_t addr, size_t size);

static contiguous_allocator_t linear_allocator;
static contiguous_allocator_t near_allocator;
static contiguous_allocator_t contig_phys_allocator;

//
// Contiguous physical memory allocator

void *mm_alloc_contiguous(size_t size)
{
    return (void*)(uint64_t)
            alloc_linear(&contig_phys_allocator, size);
}

void mm_free_contiguous(void *addr, size_t size)
{
    release_linear(&contig_phys_allocator,
                   (linaddr_t)addr, size);
}

//
// Physical page memory allocator

static void mmu_free_phys(physaddr_t addr)
{
    PROFILE_PHYS_ALLOC_ONLY( uint64_t profile_st = cpu_rdtsc() );

    uint64_t volatile *chain =
            phys_next_free + (addr < 0x100000000UL);
    uint64_t index = (addr - 0x100000UL) >> PAGE_SIZE_BIT;
    uint64_t old_next = *chain;
    for (;;) {
        assert(index < phys_alloc_count);
        phys_alloc[index] = (int32_t)old_next;
        uint64_t next_version =
                (old_next & 0xFFFFFFFF00000000UL) +
                0x100000000UL;

        uint64_t cur_next = atomic_cmpxchg(
                    chain,
                    old_next,
                    index | next_version);

        if (cur_next == old_next) {
            atomic_inc(&free_page_count);

#if DEBUG_PHYS_ALLOC
            printdbg("Free phys page addr=%lx\n", addr);
#endif
            break;
        }

        old_next = cur_next;

        pause();
    }

    PROFILE_PHYS_ALLOC_ONLY( printdbg("Free phys page %ld cycles\n",
                                      cpu_rdtsc() - profile_st) );
}

static physaddr_t mmu_alloc_phys(int low)
{
    PROFILE_PHYS_ALLOC_ONLY( uint64_t profile_st = cpu_rdtsc() );

    // Use low memory immediately if high
    // memory is exhausted
    low |= (phys_next_free[0] == 0);

    uint64_t volatile *chain = phys_next_free + !!low;

    uint64_t index_version = *chain;
    for (;;) {
        uint64_t next_version =
                (index_version & 0xFFFFFFFF00000000UL) +
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

        uint64_t cur_next = atomic_cmpxchg(
                    chain, index_version, new_next);

        if (cur_next == index_version) {
            atomic_dec(&free_page_count);

            phys_alloc[index] = 0;
            physaddr_t addr = (((physaddr_t)index) <<
                               PAGE_SIZE_BIT) + 0x100000;

#if DEBUG_PHYS_ALLOC
            printdbg("Allocated phys page addr=%lx\n", addr);
#endif

            PROFILE_PHYS_ALLOC_ONLY(
                        printdbg("Alloc phys page %ld cycles\n",
                                 cpu_rdtsc() - profile_st) );

            return addr;
        }

        index_version = cur_next;

        pause();
    }
}

//
// Path to PTE

static inline void path_from_addr(unsigned *path, linaddr_t addr)
{
    path[0] = (addr >> 39) & 0x1FF;
    path[1] = (addr >> 30) & 0x1FF;
    path[2] = (addr >> 21) & 0x1FF;
    path[3] = (addr >> 12) & 0x1FF;
}

// Returns true on innermost level carry
static inline int path_inc(unsigned *path)
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

    return path[3] == 0;
}

// Returns the linear addresses of the page tables for
// the given path
static inline void pte_from_path(pte_t **pte, unsigned *path)
{
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

    pte[0] = PT0_PTR + indices[0];
    pte[1] = PT1_PTR + indices[1];
    pte[2] = PT2_PTR + indices[2];
    pte[3] = PT3_PTR + indices[3];
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
    size_t usable_count = 0;

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

    physmem_range_t *last_range = mem_ranges +
            usable_mem_ranges - 1;
    physaddr_t addr = last_range->base +
            last_range->size - PAGE_SIZE;

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
    path_from_addr(path, 0xFFFFFFFF80000000UL - PAGE_SIZE);

    for (unsigned level = 0; level < 3; ++level)
        pte = (pte_t*)(pte[path[level]] & PTE_ADDR);

    return pte + path[3];
}

//
// Aliasing page table mapping

static pte_t *mm_map_aliasing_pte(
        pte_t *aliasing_pte, physaddr_t addr)
{
    linaddr_t linaddr;

    if ((linaddr_t)aliasing_pte >= PT_MAX_ADDR) {
        uintptr_t pt3_index = aliasing_pte - PT3_PTR;
        linaddr = (pt3_index << PAGE_SIZE_BIT);
        linaddr |= -(linaddr >> 47) << 47;
    } else {
        linaddr = 0xFFFFFFFF80000000 - PAGE_SIZE;
    }

    *aliasing_pte = (*aliasing_pte & ~PTE_ADDR) |
            (addr & PTE_ADDR) |
            (PTE_PRESENT | PTE_WRITABLE);

    cpu_invalidate_page(linaddr);

    return (pte_t*)linaddr;
}

static pte_t *init_map_aliasing_pte(
        pte_t *aliasing_pte, physaddr_t addr)
{
    return mm_map_aliasing_pte(aliasing_pte, addr);
}

//
// Page table creation

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

static int addr_present(uintptr_t addr,
                        unsigned *path, pte_t **ptes)
{
    path_from_addr(path, addr);
    return path_present(path, ptes);
}

static void mmu_map_page(
        linaddr_t addr,
        physaddr_t physaddr, pte_t flags)
{
    unsigned path[4];
    pte_t *pte_linaddr[4];
    path_from_addr(path, addr);
    int present_mask = path_present(path, pte_linaddr);

    if (unlikely((present_mask & 0x07) != 0x07)) {
        for (int i = 0; i < 4; ++i) {
            pte_t pte = *(pte_linaddr[i]);
            if (!pte) {
                physaddr_t ptaddr = init_take_page(0);
                *pte_linaddr[i] = ptaddr | PTE_PRESENT | PTE_WRITABLE;
            }
        }
    }

    *pte_linaddr[3] = (physaddr & PTE_ADDR) | flags;
}

static void mmu_tlb_perform_shootdown(void)
{
    if (cpuid_has_invpcid()) {
        //
        // Flush all with pcid

        cpu_invalidate_pcid(2, 0, 0);
    } else if (cpuid_has_pge()) {
        //
        // Flush all global by toggling global mappings

        // Toggle PGE off and on
        cpu_cr4_change_bits(CR4_PGE, 0);
        cpu_cr4_change_bits(0, CR4_PGE);
    } else {
        //
        // Flush entire TLB

        cpu_flush_tlb();
    }
}

// TLB shootdown IPI
static isr_context_t *mmu_tlb_shootdown_handler(int intr, isr_context_t *ctx)
{
    assert(intr == INTR_TLB_SHOOTDOWN);

    int cpu_number = thread_cpu_number();

    // Clear pending
    atomic_and(&shootdown_pending, ~(1 << cpu_number));

    mmu_tlb_perform_shootdown();

    return ctx;
}

static void mmu_send_tlb_shootdown(void)
{
    int cpu_count = thread_cpu_count();
    if (unlikely(cpu_count == 0))
        return;

    int irq_was_enabled = cpu_irq_disable();
    int cur_cpu = thread_cpu_number();
    int all_cpu_mask = (1 << cpu_count) - 1;
    int cur_cpu_mask = 1 << cur_cpu;
    int other_cpu_mask = all_cpu_mask & ~cur_cpu_mask;
    int old_pending = atomic_or(&shootdown_pending, other_cpu_mask);
    int need_ipi_mask = old_pending & other_cpu_mask;

    if (other_cpu_mask) {
        if (need_ipi_mask == other_cpu_mask) {
            // Send to all other CPUs
            thread_send_ipi(-1, INTR_TLB_SHOOTDOWN);
        } else {
            int cpu_mask = 1;
            for (int cpu = 0; cpu < cpu_count; ++cpu, cpu_mask <<= 1) {
                if (!(old_pending & cpu_mask) && (need_ipi_mask & cpu_mask))
                    apic_send_ipi(cpu, INTR_TLB_SHOOTDOWN);
            }
        }
    }
    cpu_irq_toggle(irq_was_enabled);
}

static isr_context_t *mmu_lazy_tlb_shootdown(isr_context_t *ctx)
{
    thread_set_cpu_mmu_seq(mmu_seq);
    cpu_flush_tlb();

    // Restart instruction
    return ctx;
}

// Page fault
static isr_context_t *mmu_page_fault_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_EX_PAGE);

    atomic_inc(&page_fault_count);

    uintptr_t fault_addr = cpu_get_fault_address();

#if DEBUG_PAGE_FAULT
    printdbg("Page fault at %lx\n", fault_addr);
#endif

    unsigned path[4];
    pte_t *ptes[4];

    int present_mask = addr_present(fault_addr, path, ptes);

    pte_t pte = present_mask == 0x07 ? *ptes[3] : 0;

    // See if process page directory is stale
    if (path[0] >= 258) {
        if (current_pagedir[path[0]] != master_pagedir[path[0]]) {
            // Update process page directory
            current_pagedir[path[0]] = master_pagedir[path[0]];
            return ctx;
        }
    }

    // Check for lazy TLB shootdown
    if (present_mask == 0x0F &&
            (pte & PTE_ADDR) != PTE_ADDR &&
            thread_get_cpu_mmu_seq() != mmu_seq)
        return mmu_lazy_tlb_shootdown(ctx);

    // If the page table exists
    if (present_mask == 0x07) {
        // If it is lazy allocated
        if ((pte & (PTE_ADDR | PTE_EX_DEVICE)) == PTE_ADDR) {
            // Allocate a page
            physaddr_t page = mmu_alloc_phys(0);

            assert(page != 0);

#if DEBUG_PAGE_FAULT
            printdbg("Assigning %lx with page %lx\n",
                     fault_addr, page);
#endif

            pte_t page_flags;

            if (ctx->gpr->info.error_code & CTX_ERRCODE_PF_W)
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
        } else if (pte & PTE_EX_DEVICE) {
            //
            // Device mapping

            linaddr_t rounded_addr = (linaddr_t)fault_addr &
                    -(intptr_t)PAGE_SIZE;

            // Lookup the device mapping
            intptr_t device = binary_search(
                        mm_dev_mappings, mm_dev_mapping_count,
                        sizeof(*mm_dev_mappings),
                        (void*)rounded_addr,
                        mm_dev_map_search, 0, 1);
            if (unlikely(device < 0))
                return 0;

            mmap_device_mapping_t *mapping = mm_dev_mappings + device;

            uint64_t mapping_offset = (char*)rounded_addr -
                    (char*)mapping->base_addr;

            // Round down to nearest 64KB boundary
            mapping_offset &= -0x10000;

            rounded_addr = (linaddr_t)mapping->base_addr + mapping_offset;

            pte_t volatile *vpte = ptes[3];

            // Attempt to be the first CPU to start reading a block
            mutex_lock(&mapping->lock);
            while (mapping->active_read >= 0 &&
                   !(*vpte & PTE_PRESENT))
                condvar_wait(&mapping->done_cond, &mapping->lock);

            // If the page became present while waiting, then done
            if (*vpte & PTE_PRESENT) {
                mutex_unlock(&mapping->lock);
                return ctx;
            }

            // Become the reader for this mapping
            mapping->active_read = mapping_offset;
            mutex_unlock(&mapping->lock);

            mapping->callback(mapping->context, (void*)rounded_addr,
                              mapping_offset, 0x10000);

            // Mark the range present from end to start
            addr_present(rounded_addr, path, ptes);
            for (size_t i = (0x10000 >> PAGE_SIZE_BIT); i > 0; --i)
                atomic_or(ptes[3] + (i - 1), PTE_PRESENT);

            mutex_lock(&mapping->lock);
            mapping->active_read = -1;
            mutex_unlock(&mapping->lock);
            condvar_wake_all(&mapping->done_cond);

            // Restart the instruction
            return ctx;
        } else {
            printdbg("Invalid page fault at 0x%zx\n", fault_addr);
            assert(!"Invalid page fault");
        }
    } else if (present_mask != 0x0F) {
        assert(!"Invalid page fault path");
    } else {
        printdbg("#PF: present=%d\n"
                 "     write=%d\n"
                 "     user=%d\n"
                 "     reserved bit violation=%d\n"
                 "     instruction fetch=%d\n"
                 "     protection key violation=%d\n"
                 "     SGX violation=%d\n"
                 "PTE: present=%d\n"
                 "     writable=%d\n"
                 "     user=%d\n"
                 "     write through=%d\n"
                 "     cache disable=%d\n"
                 "     accessed=%d\n"
                 "     dirty=%d\n"
                 "     PAT=%d\n"
                 "     global=%d\n"
                 "     physaddr=%lx\n"
                 "     no execute=%d\n"
                 "------------------\n",
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_P),
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_W),
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_U),
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_R),
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_I),
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_PK),
                 !!(ctx->gpr->info.error_code & CTX_ERRCODE_PF_SGX),
                 !!(pte & PTE_PRESENT),
                 !!(pte & PTE_WRITABLE),
                 !!(pte & PTE_USER),
                 !!(pte & PTE_PWT),
                 !!(pte & PTE_PCD),
                 !!(pte & PTE_ACCESSED),
                 !!(pte & PTE_DIRTY),
                 !!(pte & PTE_PTEPAT),
                 !!(pte & PTE_GLOBAL),
                 (pte & PTE_ADDR),
                 !!(pte & PTE_NX));

        assert(!"Unexpected page fault");
    }

    return 0;
}

#if DEBUG_ADDR_ALLOC
static int dump_addr_node(rbtree_t *tree,
                           rbtree_t::kvp_t *kvp,
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
    tree->walk(dump_addr_node, 0);
    printdbg("---------------------------------\n");
}
#endif

//
// Initialization


static int mmu_have_pat(void)
{
    // 0 == unknown, 1 == supported, -1 == not supported
    static int supported;
    if (likely(supported != 0))
        return supported > 0;

    supported = (cpuid_edx_bit(16, 1, 0) << 1) - 1;

    return supported > 0;
}

static void mmu_configure_pat(void)
{
    if (likely(mmu_have_pat()))
        msr_set(MSR_IA32_PAT, PAT_CFG);
}

void mmu_init(int ap)
{
    if (ap) {
        cpu_set_page_directory(root_physaddr);
        return;
    }

    // Hook IPI for TLB shootdown
    intr_hook(INTR_TLB_SHOOTDOWN, mmu_tlb_shootdown_handler);

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
    printdbg("Usable pages = %lu (%luMB) range_pages=%ld\n",
             usable_pages, usable_pages >> (20 - PAGE_SIZE_BIT),
             highest_usable);

    //
    // Move all page tables into upper addresses and recursive map

    pte_t *aliasing_pte = init_find_aliasing_pte();

    physaddr_t phys_addr[4];
    //physaddr_t recursive_maps[3];
    pte_t *pt;

    // Create the new root

    // Get a page
    phys_addr[0] = init_take_page(0);
    assert(phys_addr[0] != 0);

    pte_t *old_root = (pte_t*)(cpu_get_page_directory() & PTE_ADDR);

    pt = init_map_aliasing_pte(aliasing_pte, phys_addr[0]);
    memcpy(pt, old_root, PAGESIZE);

    pte_t ptflags = PTE_PRESENT | PTE_WRITABLE;// | PTE_GLOBAL;

    phys_addr[0] = cpu_get_page_directory() & PTE_ADDR;

    pt = init_map_aliasing_pte(aliasing_pte, phys_addr[0]);
    assert(pt[PT_RECURSE] == 0);
    pt[PT_RECURSE] = phys_addr[0] | ptflags;

    root_physaddr = phys_addr[0];

    mmu_configure_pat();

    //pt = init_map_aliasing_pte(aliasing_pte, root_physaddr);
    cpu_set_page_directory(phys_addr[0]);

    // Make zero page not present to catch null pointers
    *PT3_PTR = 0;
    cpu_invalidate_page(0);

    phys_alloc_count = highest_usable;
    phys_alloc = (uint32_t*)mmap(0, phys_alloc_count * sizeof(*phys_alloc),
         PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    printdbg("Building physical memory free list\n");

    memset(phys_alloc, 0, phys_alloc_count * sizeof(*phys_alloc));

    // Get the highest address taken by the kernel
    physaddr_t top_of_kernel = ___top_physaddr;

    // Reserve 4MB low memory for contiguous
    // physical allocator
    physaddr_t contiguous_start = top_of_kernel;
    top_of_kernel += 4 << 20;

    // Put all of the remaining physical memory into the free lists
    uintptr_t free_count = 0;
    for (; ; ++free_count) {
        physaddr_t addr = init_take_page(0);

        assert(addr != 0);

        if (addr > top_of_kernel &&
                addr < 0xFFFFFFFFFFFFFFFFUL) {
            uint64_t volatile *chain = phys_next_free +
                    (addr < 0x100000000UL);
            addr -= 0x100000U;
            addr >>= PAGE_SIZE_BIT;
            assert(addr < phys_alloc_count);
            phys_alloc[addr] = *chain;
            *chain = addr;

            ++free_page_count;
        } else {
            break;
        }
    }

    // Start using physical memory allocator
    usable_mem_ranges = 0;

    printdbg("%lu pages free (%luMB)\n",
           free_count,
           free_count >> (20 - PAGE_SIZE_BIT));

    intr_hook(INTR_EX_PAGE, mmu_page_fault_handler);

    callout_call('M');

    // Prepare 4MB contiguous physical memory
    // allocator with a capacity of 128
    contiguous_allocator_create(
                &contig_phys_allocator,
                &contiguous_start, 4 << 20, 128);

    contiguous_allocator_create(&linear_allocator,
                                (linaddr_t*)&linear_base,
                                PT_BASEADDR - linear_base,
                                1 << 20);

    contiguous_allocator_create(&near_allocator,
                                (linaddr_t*)&near_base,
                                0ULL - near_base,
                                128);

    // Allocate guard page
    alloc_linear(&linear_allocator, PAGE_SIZE);

    // Alias master page directory to be accessible
    // from all process contexts
    master_pagedir = (pte_t*)mmap((void*)root_physaddr, PAGE_SIZE, PROT_READ,
                          MAP_PHYSICAL, -1, 0);

    current_pagedir = (pte_t*)(PT0_PTR);
}

static size_t round_up(size_t n)
{
    return (n + PAGE_MASK) & (int)-PAGE_SIZE;
}

static size_t round_down(size_t n)
{
    return n & (int)-PAGE_MASK;
}

//
// Linear address allocator

static int contiguous_allocator_cmp_key(
        typename rbtree_t<>::kvp_t const *lhs,
        typename rbtree_t<>::kvp_t const *rhs,
        void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            0;
}

static int contiguous_allocator_cmp_both(
        typename rbtree_t<>::kvp_t const *lhs,
        typename rbtree_t<>::kvp_t const *rhs,
        void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            lhs->val < rhs->val ? -1 :
            rhs->val < lhs->val ? 1 :
            0;
}

#if DEBUG_LINEAR_SANITY
static void sanity_check_by_size(rbtree_t *tree)
{
    static int call = 0;
    ++call;

    rbtree_t::kvp_t prev = { 0, 0 };
    rbtree_t::kvp_t curr;

    for (tree->iter_t it = rbtree_first(0);
         it;
         it = tree->next(it)) {
        curr = tree->item(it);
        assert(prev.val + prev.key != curr.val);
        prev = curr;
    }
}

static void sanity_check_by_addr(rbtree_t *tree)
{
    rbtree_t::kvp_t prev = { 0, 0 };
    rbtree_t::kvp_t curr;

    for (rbtree_t::iter_t it = tree->first(0);
         it;
         it = tree->next(it)) {
        curr = tree->item(it);
        assert(prev.key + prev.val != curr.key);
        prev = curr;
    }
}
#endif

static void contiguous_allocator_create(
        contiguous_allocator_t *allocator,
        linaddr_t *addr, size_t size, size_t capacity)
{
    linaddr_t initial_addr = *addr;

    mutex_init(&allocator->free_addr_lock);
    allocator->free_addr_by_addr = rbtree_t<>::create(
                contiguous_allocator_cmp_key, 0, capacity);
    allocator->free_addr_by_size = rbtree_t<>::create(
                contiguous_allocator_cmp_both, 0, capacity);

    // Account for space taken creating trees

    size_t size_adj = *addr - initial_addr;
    size -= size_adj;

    allocator->free_addr_by_size->insert(size, *addr);
    allocator->free_addr_by_addr->insert(*addr, size);
}

static uintptr_t alloc_linear(
        contiguous_allocator_t *allocator,
        size_t size)
{
    // Round up to a multiple of the page size
    size = round_up(size);

    linaddr_t addr;

    if (allocator->free_addr_by_addr &&
            allocator->free_addr_by_size) {
        mutex_lock(&allocator->free_addr_lock);

#if DEBUG_ADDR_ALLOC
        printdbg("---- Alloc %lx\n", size);
        dump_addr_tree(allocator->free_addr_by_addr,
                       "Addr map by addr (before alloc)");
        //dump_addr_tree(allocator->free_addr_by_size,
        //               "Addr map by size (before alloc)");
#endif

        // Find the lowest address item that is big enough
        typename rbtree_t<>::iter_t place =
                allocator->free_addr_by_size->lower_bound(size, 0);

        typename rbtree_t<>::kvp_t by_size =
                allocator->free_addr_by_size->item(place);

        if (by_size.key < size) {
            place = allocator->free_addr_by_size->next(place);
            by_size = allocator->free_addr_by_size->item(place);
        }

        allocator->free_addr_by_size->delete_at(place);

        // Delete corresponding entry by address
        allocator->free_addr_by_addr->delete_item(by_size.val, by_size.key);

        if (by_size.key > size) {
            // Insert remainder by size
            allocator->free_addr_by_size->insert(
                        by_size.key - size, by_size.val + size);

            // Insert remainder by address
            allocator->free_addr_by_addr->insert(
                        by_size.val + size, by_size.key - size);
        }

        addr = by_size.val;

#if DEBUG_ADDR_ALLOC
        dump_addr_tree(allocator->free_addr_by_addr,
                       "Addr map by addr (after alloc)");
        //dump_addr_tree(allocator->free_addr_by_size,
        //    "Addr map by size (after alloc)");

        printdbg("%lx ----\n", addr);
#endif

#if DEBUG_LINEAR_SANITY
        sanity_check_by_size(allocator->free_addr_by_size);
        sanity_check_by_addr(allocator->free_addr_by_addr);
#endif

#if DEBUG_ADDR_ALLOC
        printdbg("Took address space @ %lx,"
                 " size=%lx\n", addr, size);
#endif

        mutex_unlock(&allocator->free_addr_lock);
    } else {
        addr = atomic_xadd(&linear_base, size);
        printdbg("Took early address space @ %lx,"
                 " size=%lx,"
                 " new linear_base=%lx\n", addr, size, linear_base);
    }

    return addr;
}

static void take_linear(
        contiguous_allocator_t *allocator,
        linaddr_t addr,
        size_t size)
{
    assert(allocator->free_addr_by_addr);
    assert(allocator->free_addr_by_size);

    mutex_lock(&allocator->free_addr_lock);

    // Round to pages
    addr &= -PAGE_SIZE;
    size = round_up(size);

    linaddr_t end = addr + size;

    // Find the last free range before or at the address
    typename rbtree_t<>::iter_t place =
            allocator->free_addr_by_addr->lower_bound(addr, 0);

    typename rbtree_t<>::iter_t next_place;

    for (; place; place = next_place) {
        typename rbtree_t<>::kvp_t by_addr =
                allocator->free_addr_by_addr->item(place);

        if (by_addr.key < addr && by_addr.key + by_addr.val > addr) {
            //
            // The found free block is before the range and overlaps it

            // Save next block
            next_place = allocator->free_addr_by_addr->next(place);

            // Delete the size entry
            allocator->free_addr_by_size->delete_item(
                        by_addr.val, by_addr.key);

            // Delete the address entry
            allocator->free_addr_by_addr->delete_at(place);

            // Create a smaller block that does not overlap taken range
            by_addr.val = addr - by_addr.key;

            // Insert smaller range by address
            allocator->free_addr_by_addr->insert_pair(&by_addr);

            // Insert smaller range by size
            allocator->free_addr_by_size->insert(by_addr.val, by_addr.key);
        } else if (by_addr.key >= addr && by_addr.key + by_addr.val <= end) {
            //
            // Range completely covers block, delete block

            next_place = allocator->free_addr_by_addr->next(place);

            allocator->free_addr_by_size->delete_item(
                        by_addr.val, by_addr.key);

            allocator->free_addr_by_addr->delete_at(place);
        } else if (by_addr.key > addr) {
            //
            // Range cut off some of beginning of block

            allocator->free_addr_by_size->delete_item(
                          by_addr.val, by_addr.key);

            allocator->free_addr_by_addr->delete_at(place);

            break;
        } else {
            assert(by_addr.key >= end);

            // Block is past end of allocated range, done
            break;
        }
    }
}

static void release_linear(
        contiguous_allocator_t *allocator,
        uintptr_t addr, size_t size)
{
    // Round address down to page boundary
    size_t misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;

    // Round size up to multiple of page size
    size = round_up(size);

    linaddr_t end = addr + size;

    mutex_lock(&allocator->free_addr_lock);

#if DEBUG_ADDR_ALLOC
    printdbg("---- Free %lx @ %lx\n", size, addr);
    dump_addr_tree(allocator->free_addr_by_addr,
                   "Addr map by addr (before free)");
    //dump_addr_tree(allocator->free_addr_by_size,
    //               "Addr map by size (before free)");
#endif

    // Find the nearest free block before the freed range
    typename rbtree_t<>::iter_t pred_it =
            allocator->free_addr_by_addr->lower_bound(addr, 0);

    // Find the nearest free block after the freed range
    typename rbtree_t<>::iter_t succ_it =
            allocator->free_addr_by_addr->lower_bound(end, ~0UL);

    typename rbtree_t<>::kvp_t pred =
            allocator->free_addr_by_addr->item(pred_it);
    typename rbtree_t<>::kvp_t succ =
            allocator->free_addr_by_addr->item(succ_it);

    int coalesce_pred = ((pred.key + pred.val) == addr);
    int coalesce_succ = (succ.key == end);

    if (coalesce_pred) {
        addr -= pred.val;
        size += pred.val;
        allocator->free_addr_by_addr->delete_at(pred_it);

        allocator->free_addr_by_size->delete_item(pred.val, pred.key);
    }

    if (coalesce_succ) {
        size += succ.val;
        allocator->free_addr_by_addr->delete_at(succ_it);

        allocator->free_addr_by_size->delete_item(succ.val, succ.key);
    }

    allocator->free_addr_by_size->insert(size, addr);
    allocator->free_addr_by_addr->insert(addr, size);

#if DEBUG_ADDR_ALLOC
    dump_addr_tree(allocator->free_addr_by_addr,
                   "Addr map by addr (after free)");
    //dump_addr_tree(allocator->free_addr_by_size,
    /                "Addr map by size (after free)");
#endif

    mutex_unlock(&allocator->free_addr_lock);
}

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
    PROFILE_MMAP_ONLY( uint64_t profile_st = cpu_rdtsc() );

    // Must pass MAP_DEVICE if passing a device registration index
    assert((flags & MAP_DEVICE) || (fd < 0));

    // Not used (yet)
    assert(offset == 0);

    assert(len > 0);

#if DEBUG_PAGE_TABLES
    printdbg("Mapping len=%zx prot=%x flags=%x addr=%lx\n",
             len, prot, flags, (uintptr_t)addr);
#endif

    pte_t page_flags = 0;

    if (unlikely(flags & MAP_INVALID_MASK))
        return 0;

    if (unlikely(flags & (MAP_STACK | MAP_32BIT)))
        flags |= MAP_POPULATE;

    if (unlikely(flags & MAP_PHYSICAL))
        page_flags |= PTE_EX_PHYSICAL | PTE_PRESENT;

    if (unlikely(flags & MAP_WEAKORDER)) {
        if (likely(mmu_have_pat()))
            page_flags |= PTE_PTEPAT_n(PAT_IDX_WC);
        else
            page_flags |= PTE_PCD | PTE_PWT;
    } else {
        if (unlikely(flags & MAP_NOCACHE))
            page_flags |= PTE_PCD;

        if (unlikely(flags & MAP_WRITETHRU))
            page_flags |= PTE_PWT;
    }

    if (flags & MAP_POPULATE)
        page_flags |= PTE_PRESENT;

    if (unlikely(flags & MAP_DEVICE))
        page_flags |= PTE_EX_DEVICE;

    if (flags & MAP_USER)
        page_flags |= PTE_USER;

    if ((flags & MAP_GLOBAL) || !(flags & MAP_USER))
        page_flags |= PTE_GLOBAL;

    if (prot & PROT_WRITE)
        page_flags |= PTE_WRITABLE;

    if (!(prot & PROT_EXEC) && cpuid_has_nx())
        page_flags |= PTE_NX;

    uintptr_t misalignment = 0;

    if (unlikely(flags & MAP_PHYSICAL)) {
        misalignment = (uintptr_t)addr & PAGE_MASK;
        len += misalignment;
    }

    PROFILE_LINEAR_ALLOC_ONLY( uint64_t profile_linear_st = cpu_rdtsc() );
    linaddr_t linear_addr;
    if (!addr || (flags & MAP_PHYSICAL)) {
        if (!(flags & MAP_NEAR))
            linear_addr = alloc_linear(&linear_allocator, len);
        else
            linear_addr = alloc_linear(&near_allocator, len);
    } else {
        linear_addr = (linaddr_t)addr;
        take_linear(&linear_allocator, linear_addr, len);
    }
    PROFILE_LINEAR_ALLOC_ONLY( printdbg("Allocation of %lu bytes of"
                                        " address space took %lu cycles\n",
                                        len, cpu_rdtsc() - profile_linear_st) );

    assert(linear_addr > 0x100000);

    for (size_t ofs = 0; ofs < len; ofs += PAGE_SIZE)
    {
        if (likely(!(flags & MAP_PHYSICAL))) {
            // Allocate normal memory

            // Not present pages with max physaddr are demand committed
            physaddr_t page = PTE_ADDR;

            // If populating, assign physical memory immediately
            // Always commit first page immediately
            if (!ofs || unlikely(flags & MAP_POPULATE)) {
                page = init_take_page(!!(flags & MAP_32BIT));
                assert(page != 0);
            }

            mmu_map_page(linear_addr + ofs, page, page_flags |
                         (!ofs & PTE_PRESENT));
        } else {
            // addr is a physical address, caller uses
            // returned linear address to access it
            mmu_map_page(linear_addr + ofs,
                         (((physaddr_t)addr) + ofs) & PTE_ADDR,
                         page_flags);
        }
    }

    atomic_inc(&mmu_seq);

    assert(linear_addr > 0x100000);

    PROFILE_MMAP_ONLY( printdbg("mmap of %zd bytes took %ld cycles\n",
                                len, cpu_rdtsc() - profile_st); );

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

    if (!(flags & MREMAP_FIXED)) {
        va_list ap;
        va_start(ap, flags);
        new_address = va_arg(ap, void*);
        va_end(ap);
    }
    (void)new_address;

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

        new_linear = alloc_linear(&linear_allocator, new_size);

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

    release_linear(&linear_allocator,
                   (linaddr_t)addr - misalignment,
                   size);

    return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
    // Fail on invalid protection mask
    if (unlikely(prot != (prot & (PROT_READ | PROT_WRITE | PROT_EXEC))))
        return -1;

    // Fail on invalid address
    if (!addr)
        return -1;

    unsigned misalignment = (uintptr_t)addr & PAGE_MASK;
    addr = (char*)addr - misalignment;
    len += misalignment;

    len = (len + PAGE_MASK) & -(int)PAGE_SIZE;

    if (unlikely(len == 0))
        return 0;

    /// Demand paged PTE, readable
    ///  present=0, addr=PTE_ADDR
    /// Demand paged PTE, not readable
    ///  present=0, addr=(PTE_ADDR>>1)&PTE_ADDR

    // Only the MSB of the physical address set
    pte_t const demand_no_read = (PTE_ADDR >> 1) & PTE_ADDR;

    pte_t no_exec = cpuid_has_nx() ? PTE_NX : 0;

    // Bits to set in PTE
    pte_t set_bits =
            ((prot & PROT_EXEC)
             ? 0
             : no_exec) |
            ((prot & PROT_READ)
             ? PTE_PRESENT
             : 0) |
            ((prot & PROT_WRITE)
             ? PTE_WRITABLE
             : 0);

    // Bits to clear in PTE
    // If the protection is no-read,
    // we clear the highest bit in the physical address.
    // This invalidates the demand paging value without
    // invalidating the
    pte_t clr_bits =
            ((prot & PROT_EXEC)
             ? no_exec
             : 0) |
            ((prot & PROT_READ)
             ? 0
             : PTE_PRESENT) |
            ((prot & PROT_WRITE)
             ? 0
             : PTE_WRITABLE);

    unsigned path[4];
    unsigned path_en[4];

    path_from_addr(path, (linaddr_t)addr);
    path_from_addr(path_en, (linaddr_t)addr + len);

    pte_t *pt[4];
    pte_t *pt_en[4];
    pte_from_path(pt, path);
    pte_from_path(pt_en, path_en);

    while (((pt[0] != pt_en[0]) | (pt[1] != pt_en[1]) |
            (pt[2] != pt_en[2]) | (pt[3] != pt_en[3])) &&
           (*pt[0] & PTE_PRESENT) &&
           (*pt[1] & PTE_PRESENT) &&
           (*pt[2] & PTE_PRESENT)) {
        pte_t replace;
        for (pte_t expect = *pt[3]; ; pause()) {
            int demand_paged = ((expect & demand_no_read) == demand_no_read);
            if (expect == 0)
                return -1;
            else if (demand_paged && (prot & PROT_READ))
                // We are enabling read on demand paged entry
                replace = (expect & ~clr_bits) |
                        ((set_bits & ~PTE_PRESENT) | PTE_ADDR);
            else if (demand_paged && !(prot & PROT_READ))
                // We are disabling read on a demand paged entry
                replace = (expect & demand_no_read & ~clr_bits) |
                        (set_bits & ~PTE_PRESENT);
            else
                // Just change permission bits
                replace = (expect & ~clr_bits) | set_bits;

            // Try to update PTE
            if (atomic_cmpxchg_upd(pt[3], &expect, replace))
                break;
        }

        cpu_invalidate_page((uintptr_t)addr);
        addr = (char*)addr + PAGE_SIZE;

        ++pt[3];
        path_inc(path);
        pte_from_path(pt, path);
    }

    return 1;
}

// Support discarding pages and reverting to demand
// paged state with MADV_DONTNEED.
// Support enabling/disabling write combining
// with MADV_WEAKORDER/MADV_STRONGORDER
int madvise(void *addr, size_t len, int advice)
{
    unsigned path[4];
    unsigned path_en[4];

    if (unlikely(len == 0))
        return 0;

    pte_t order_bits = 0;

    switch (advice) {
    case MADV_WEAKORDER:
        order_bits = PTE_PTEPAT_n(PAT_IDX_WC);
        break;

    case MADV_STRONGORDER:
        order_bits = PTE_PTEPAT_n(PAT_IDX_WB);
        break;

    case MADV_DONTNEED:
        order_bits = -1;
        break;

    default:
        return 0;
    }

    path_from_addr(path, (linaddr_t)addr);
    path_from_addr(path_en, (linaddr_t)addr + len - 1);

    pte_t *pt[4];
    pte_from_path(pt, path);

    pte_t const demand_mask = (PTE_ADDR >> 1) & PTE_ADDR;

    while (((path[0] != path_en[0]) | (path[1] != path_en[1]) |
            (path[2] != path_en[2]) | (path[3] != path_en[3])) &&
           (*pt[0] & PTE_PRESENT) &&
           (*pt[1] & PTE_PRESENT) &&
           (*pt[2] & PTE_PRESENT)) {
        pte_t replace;
        for (pte_t expect = *pt[3]; ; pause()) {
            if (order_bits == (pte_t)-1) {
                physaddr_t page = 0;
                if (expect && (expect & demand_mask) != demand_mask) {
                    page = expect & PTE_ADDR;
                    replace = expect | PTE_ADDR;

                    if (atomic_cmpxchg_upd(pt[3], &expect, replace)) {
                        mmu_free_phys(page);
                        break;
                    }
                }
            } else {
                replace = (expect & ~(PTE_PTEPAT | PTE_PCD | PTE_PWT)) |
                        order_bits;

                if (atomic_cmpxchg_upd(pt[3], &expect, replace))
                    break;
            }
        }

        cpu_invalidate_page((uintptr_t)addr);
        addr = (char*)addr + PAGE_SIZE;

        path_inc(path);
        pte_from_path(pt, path);
    }

    mmu_send_tlb_shootdown();

    return 0;
}

uintptr_t mphysaddr(void volatile *addr)
{
    linaddr_t linaddr = (linaddr_t)addr;

    uintptr_t misalignment = linaddr & PAGE_MASK;

    unsigned path[4];
    pte_t *ptes[4];
    int present_mask = addr_present(linaddr, path, ptes);

    if ((present_mask & 0x07) != 0x07)
        return 0;

    pte_t pte = *ptes[3];
    physaddr_t page = pte & PTE_ADDR;

    // If page is being demand paged
    if (page == PTE_ADDR) {
        // Commit a page
        page = mmu_alloc_phys(0);

        pte_t new_pte = (pte & ~PTE_ADDR) | page;

        if (atomic_cmpxchg_upd(ptes[3], &pte, new_pte))
            pte = new_pte;
        else
            mmu_free_phys(page);
    } else if (!(pte & PTE_PRESENT)) {
        return 0;
    }

    return (pte & PTE_ADDR) + misalignment;
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
    mphysranges_state_t *state = (mphysranges_state_t *)context;

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

void *mmap_register_device(void *context,
                           uint64_t block_size,
                           uint64_t block_count,
                           int prot,
                           mm_dev_mapping_callback_t callback)
{
    spinlock_lock_noirq(&mm_dev_mapping_lock);

    mmap_device_mapping_t *mapping = 0;
    if (mm_dev_mapping_count < countof(mm_dev_mappings)) {
        mapping = mm_dev_mappings + mm_dev_mapping_count++;
        mapping->base_addr = mmap(0, block_size * block_count,
                                  prot, MAP_DEVICE,
                                  mapping - mm_dev_mappings,
                                  0);
        mapping->context = context;
        mapping->len = block_size * block_count;
        mapping->callback = callback;

        mutex_init(&mapping->lock);
        condvar_init(&mapping->done_cond);

        mapping->active_read = -1;
    }

    spinlock_unlock_noirq(&mm_dev_mapping_lock);

    return likely(mapping) ? mapping->base_addr : 0;
}

static int mm_dev_map_search(void const *v, void const *k, void *s)
{
    (void)s;
    mmap_device_mapping_t const *mapping = (mmap_device_mapping_t const *)v;

    if (k < mapping->base_addr)
        return -1;

    void const *mapping_end = (char*)mapping->base_addr + mapping->len;
    if (k > mapping_end)
        return 1;

    return 0;
}

// Create a new process context and switch to it,
// returns the physical address of the page directory
uintptr_t mm_create_process(void)
{
    pte_t *tables = (pte_t*)mmap(0, 4 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                        MAP_POPULATE, -1, 0);
    mmphysrange_t addresses[4];
    mphysranges(addresses, countof(addresses),
                tables, 4 * PAGE_SIZE, PAGE_SIZE);

    //
    // Setup recursive mapping


    // Copy kernel space
    for (int i = 256; i < 512; ++i)
        tables[i] = master_pagedir[i];

    tables[PT_RECURSE] = addresses[0].physaddr;

    cpu_set_page_directory(addresses[0].physaddr);

    return addresses[0].physaddr;
}

static void mm_free_all_user_memory(void)
{
    unsigned path[4];
    pte_t *ptes[4];
    pte_t *base[3];

    addr_present(0, path, ptes);

    for (uintptr_t m0 = 0; m0 < 256; ++m0) {
        if (!(ptes[0][m0] & PTE_PRESENT))
            continue;

        base[0] = ptes[1] + (m0 << 9);

        if (*base[0] & PTE_PRESENT)
            continue;

        for (uintptr_t m1 = 0; m1 < 512; ++m1) {
            base[1] = ptes[2] +
                    (m0 << (9*2)) +
                    (m1 << 9);

            if (*base[1] & PTE_PRESENT)
                continue;

            for (uintptr_t m2 = 0; m2 < 512; ++m2) {
                base[2] = ptes[3] +
                        (m0 << (9*3)) +
                        (m1 << (9*2)) +
                        (m2 << 9);

                if (*base[2] & PTE_PRESENT)
                    continue;

                for (uintptr_t m3 = 0; m3 < 512; ++m3) {
                    if (base[2][m3] & PTE_PRESENT)
                        mmu_free_phys(base[2][m3] & PTE_ADDR);
                }

                mmu_free_phys(*base[2] & PTE_ADDR);
            }

            mmu_free_phys(*base[1] & PTE_ADDR);
        }

        mmu_free_phys(*base[0] & PTE_ADDR);
    }

    mmu_free_phys(cpu_get_page_directory() & PTE_ADDR);
}

// Destroy the current process mapping
// Switches to the master mapping,
// frees the recursive mapping,
// and recursively frees all user memory and page tables
void mm_destroy_process(void)
{
    unsigned path[4];
    pte_t *ptes[4];
    int mask = addr_present((uintptr_t)PT0_PTR, path, ptes);

    assert(mask == 0xF);

    mm_free_all_user_memory();
}
