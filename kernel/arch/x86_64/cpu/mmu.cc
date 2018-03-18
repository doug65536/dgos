#include "mmu.h"
#include "assert.h"
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
#include "cpuid.h"
#include "thread_impl.h"
#include "threadsync.h"
#include "rbtree.h"
#include "idt.h"
#include "bsearch.h"
#include "process.h"
#include "cpu_broadcast.h"
#include "errno.h"
#include "vector.h"
#include "main.h"

// Allow G bit set in PDPT and PD in recursive page table mapping
// This causes KVM to throw #PF(reserved_bit_set|present)
#define GLOBAL_RECURSIVE_MAPPING    0

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
#define PTE_EX_WAIT_BIT     (PTE_AVAIL2_BIT+0)

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
#define PTE_EX_WAIT         (1UL << PTE_EX_WAIT_BIT)

// PAT configuration
#define PAT_IDX_WB  0
#define PAT_IDX_WT  1
#define PAT_IDX_UCW 2
#define PAT_IDX_UC  3
#define PAT_IDX_WC  4
#define PAT_IDX_WP  5

#define PAT_CFG \
    (CPU_MSR_IA32_PAT_n(PAT_IDX_WB, CPU_MSR_IA32_PAT_WB) | \
    CPU_MSR_IA32_PAT_n(PAT_IDX_WT, CPU_MSR_IA32_PAT_WT) | \
    CPU_MSR_IA32_PAT_n(PAT_IDX_UCW, CPU_MSR_IA32_PAT_UCW) | \
    CPU_MSR_IA32_PAT_n(PAT_IDX_UC, CPU_MSR_IA32_PAT_UC) | \
    CPU_MSR_IA32_PAT_n(PAT_IDX_WC, CPU_MSR_IA32_PAT_WC) | \
    CPU_MSR_IA32_PAT_n(PAT_IDX_WP, CPU_MSR_IA32_PAT_WP))

#define PTE_PDEPAT_n(idx) \
    ((PTE_PDEPAT & -!!(idx & 4)) | \
    (PTE_PCD & -!!(idx & 2)) | \
    (PTE_PWT & -!!(idx & 1)))

#define PTE_PTEPAT_n(idx) \
    ((PTE_PTEPAT & -!!(idx & 4U)) | \
    (PTE_PCD & -!!(idx & 2U)) | \
    (PTE_PWT & -!!(idx & 1U)))

// Page table entries don't have a structure, they
// are a bunch of bitfields. Use uint64_t and the
// constants above
typedef uintptr_t pte_t;

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
#define CANONICALIZE(n) uintptr_t(intptr_t(uintptr_t(n) << 16) >> 16)

#define PT3_ADDR        (CANONICALIZE(PT3_INDEX*sizeof(pte_t)))
#define PT2_ADDR        (CANONICALIZE(PT2_INDEX*sizeof(pte_t)))
#define PT1_ADDR        (CANONICALIZE(PT1_INDEX*sizeof(pte_t)))
#define PT0_ADDR        (CANONICALIZE(PT0_INDEX*sizeof(pte_t)))

#define PT3_PTR         ((pte_t*)PT3_ADDR)
#define PT2_PTR         ((pte_t*)PT2_ADDR)
#define PT1_PTR         ((pte_t*)PT1_ADDR)
#define PT0_PTR         ((pte_t*)PT0_ADDR)

#define PT_KERNBASE     0xFFFFFFFF80000000
#define PT_BASEADDR     (PT0_ADDR)
#define PT_MAX_ADDR     (PT0_ADDR + (512UL << 30))

static format_flag_info_t pte_flags[] = {
    { "XD",     1,                  0, PTE_NX_BIT       },
    { "PK",     PTE_PK_MASK,        0, PTE_PK_BIT       },
    { "62:52",  PTE_AVAIL2_MASK,    0, PTE_AVAIL2_BIT   },
    { "PADDR",  PTE_ADDR,           0, 0                },
    { "11:9",   PTE_AVAIL1_MASK,    0, PTE_AVAIL1_BIT   },
    { "G",      1,                  0, PTE_GLOBAL_BIT   },
    { "PS",     1,                  0, PTE_PAGESIZE_BIT },
    { "D",      1,                  0, PTE_DIRTY_BIT    },
    { "A",      1,                  0, PTE_ACCESSED_BIT },
    { "PCD",    1,                  0, PTE_PCD_BIT      },
    { "PWT",    1,                  0, PTE_PWT_BIT      },
    { "U",      1,                  0, PTE_USER_BIT     },
    { "W",      1,                  0, PTE_WRITABLE_BIT },
    { "P",      1,                  0, PTE_PRESENT_BIT  },
    { 0,        0,                  0, -1               }
};

static format_flag_info_t pf_err_flags[] = {
    { "Pr",         1,      0, CTX_ERRCODE_PF_P_BIT   },
    { "Wr",         1,      0, CTX_ERRCODE_PF_W_BIT   },
    { "Usr",        1,      0, CTX_ERRCODE_PF_U_BIT   },
    { "RsvdBit",    1,      0, CTX_ERRCODE_PF_R_BIT   },
    { "Insn",       1,      0, CTX_ERRCODE_PF_I_BIT   },
    { "PK",         1,      0, CTX_ERRCODE_PF_PK_BIT  },
    { "14:6",       0x1FF,  0, 6                      },
    { "SGX",        1,      0, CTX_ERRCODE_PF_SGX_BIT },
    { "31:16",      0xFFFF, 0, 16                     },
    { 0,            1,      0, -1                     }
};

static char const *pte_level_names[] = {
    "PML4",
    "PDPT",
    "PD",
    "PTE"
};

static size_t mmu_describe_pte(char *buf, size_t sz, pte_t pte)
{
    return format_flags_register(buf, sz, pte, pte_flags);
}

static size_t mmu_describe_pf(char *buf, size_t sz, int err_code)
{
    return format_flags_register(buf, sz, err_code, pf_err_flags);
}

static void mmu_dump_ptes(pte_t **ptes)
{
    char fmt_buf[64];

    bool present = true;

    for (size_t i = 0; i < 4; ++i) {
        if (present) {
            mmu_describe_pte(fmt_buf, sizeof(fmt_buf), *ptes[i]);
            printdbg("%5s: %016lx %s\n",
                     pte_level_names[i], *ptes[i], fmt_buf);
        } else {
            printdbg("%5s: <not present>\n", pte_level_names[i]);
        }

        if (!(*ptes[i] & PTE_PRESENT))
            present = false;
    }
}

static void mmu_dump_pf(uintptr_t pf)
{
    char fmt_buf[64];
    mmu_describe_pf(fmt_buf, sizeof(fmt_buf), pf);
    printdbg("#PF: %04zx %s\n", pf, fmt_buf);
}

//
// Device mapping

// Device registration for memory mapped device
struct mmap_device_mapping_t {
    void *base_addr;
    uint64_t len;
    mm_dev_mapping_callback_t callback;
    void *context;
    mutex lock;
    condition_variable done_cond;
    int64_t active_read;
};

static int mm_dev_map_search(void const *v, void const *k, void *s);

static vector<mmap_device_mapping_t*> mm_dev_mappings;
using mm_dev_mapping_lock_type = mcslock;
using mm_dev_mapping_scoped_lock = unique_lock<mm_dev_mapping_lock_type>;
static mm_dev_mapping_lock_type mm_dev_mapping_lock;

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

// Original memory map
physmem_range_t phys_mem_map[64];
size_t phys_mem_map_count;

// Memory map
static physmem_range_t mem_ranges[64];
static size_t usable_mem_ranges;

physaddr_t root_physaddr;
static pte_t const * master_pagedir;
static pte_t *current_pagedir;

class mmu_phys_allocator_t {
    typedef uint32_t entry_t;
public:
    static size_t size_from_highest_page(physaddr_t page_index);

    void init(void *addr, physaddr_t begin_,
                  uint8_t log2_pagesz_ = PAGE_SIZE_BIT);

    void add_free_space(physaddr_t base, size_t size);

    // Unreliably peek and see if there might be a free page, outside lock
    operator bool() const
    {
        return next_free != 0;
    }

    physaddr_t alloc_one(bool low);

    // Take multiple pages and receive each physical address in callback
    // Returns false with no memory allocated on failure
    template<typename F>
    bool __always_inline alloc_multiple(bool low, size_t size, F callback);

    void release_one(physaddr_t addr);

    void addref(physaddr_t addr);

    void addref_virtual_range(linaddr_t start, size_t len);

    class free_batch_t {
    public:
        void free(physaddr_t addr)
        {
            if (count == countof(pages))
                flush();
            pages[count++] = addr;
        }

        void flush()
        {
            scoped_lock lock(owner.lock);
            for (size_t i = 0; i < count; ++i)
                owner.release_one_locked(pages[i]);
            count = 0;
        }

        free_batch_t(mmu_phys_allocator_t& owner)
            : owner(owner)
            , count(0)
        {
        }

        ~free_batch_t()
        {
            if (count)
                flush();
        }

    private:
        physaddr_t pages[16];
        mmu_phys_allocator_t &owner;
        unsigned count;
    };

private:
    __always_inline size_t index_from_addr(physaddr_t addr) const
    {
        return (addr - begin) >> log2_pagesz;
    }

    __always_inline physaddr_t addr_from_index(size_t index) const
    {
        return (index << log2_pagesz) + begin;
    }

    __always_inline void release_one_locked(physaddr_t addr)
    {
        size_t index = index_from_addr(addr);
        unsigned low = addr < 0x100000000;
        if (entries[index] == 1) {
            // Free the page
            entries[index] = next_free[low];
            next_free[low] = index;
        } else {
            // Reduce reference count
            --entries[index];
        }
    }

    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;

    entry_t *entries;
    physaddr_t begin;
    entry_t next_free[2];
    entry_t free_page_count;
    lock_type lock;
    uint8_t log2_pagesz;
};

extern char ___init_brk[];
extern uintptr_t ___top_physaddr;
static linaddr_t near_base = (linaddr_t)___init_brk;
static linaddr_t volatile linear_base = PT_MAX_ADDR;

mmu_phys_allocator_t phys_allocator;

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

//
// Contiguous allocator

struct contiguous_allocator_t {
public:
    void early_init(linaddr_t *addr, size_t size, const char *name);
    void init(linaddr_t addr, size_t size, const char *name);
    uintptr_t alloc_linear(size_t size);
    bool take_linear(linaddr_t addr, size_t size, bool require_free);
    void release_linear(uintptr_t addr, size_t size);
    void dump(char const *format, ...);
private:
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;
    lock_type free_addr_lock;
    typedef rbtree_t<> tree_t;
    tree_t free_addr_by_size;
    tree_t free_addr_by_addr;
    char const *name;
};

static contiguous_allocator_t linear_allocator;
static contiguous_allocator_t near_allocator;
static contiguous_allocator_t contig_phys_allocator;
static contiguous_allocator_t hole_allocator;

//
// Contiguous physical memory allocator

uintptr_t mm_alloc_contiguous(size_t size)
{
    return contig_phys_allocator.alloc_linear(size);
}

void mm_free_contiguous(uintptr_t addr, size_t size)
{
    contig_phys_allocator.release_linear((linaddr_t)addr, size);
}

//
// Physical page memory allocator

static void mmu_free_phys(physaddr_t addr)
{
    phys_allocator.release_one(addr);
}

static physaddr_t mmu_alloc_phys(int low)
{
    physaddr_t page;

    // Try to get high/low page as specified
    page = phys_allocator.alloc_one(low);
    if (unlikely(!page))
        return 0;

    return page;
}

//
// Path to PTE

static __always_inline void path_from_addr(unsigned *path, linaddr_t addr)
{
    path[3] = (addr >>= 12) & 0x1FF;
    path[2] = (addr >>= 9) & 0x1FF;
    path[1] = (addr >>= 9) & 0x1FF;
    path[0] = (addr >>= 9) & 0x1FF;
}

static int ptes_present(pte_t **ptes)
{
    int present_mask;

    present_mask = (*ptes[0] & PTE_PRESENT);
    present_mask |= (likely(present_mask == 1) &&
                     (*ptes[1] & PTE_PRESENT)) << 1;
    present_mask |= (likely(present_mask == 3) &&
                     (*ptes[2] & PTE_PRESENT)) << 2;
    present_mask |= (likely(present_mask == 7) &&
                     (*ptes[3] & PTE_PRESENT)) << 3;

    return present_mask;
}

static __always_inline void ptes_from_addr(pte_t **pte, linaddr_t addr)
{
    addr &= 0xFFFFFFFFF000U;
    addr >>= 12;
    pte[3] = PT3_PTR + addr;
    addr >>= 9;
    pte[2] = PT2_PTR + addr;
    addr >>= 9;
    pte[1] = PT1_PTR + addr;
    addr >>= 9;
    pte[0] = PT0_PTR + addr;
}

// Returns the linear addresses of the page tables for the given path
static __always_inline void ptes_from_path(pte_t **pte, unsigned *path)
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

static int path_present(unsigned *path, pte_t **ptes)
{
    ptes_from_path(ptes, path);
    return ptes_present(ptes);
}

static int addr_present(uintptr_t addr,
                        unsigned *path, pte_t **ptes)
{
    path_from_addr(path, addr);
    return path_present(path, ptes);
}

// Returns linear page index (addr>>12) represented by the specified path
static __always_inline uintptr_t path_inc(unsigned *path)
{
    // Branchless algorithm

    uintptr_t n =
            (linaddr_t)(path[3] << (9 * 0)) |
            ((linaddr_t)path[2] << (9 * 1)) |
            ((linaddr_t)path[1] << (9 * 2)) |
            ((linaddr_t)path[0] << (9 * 3));

    ++n;

    path[3] = (n >> (9 * 0)) & 0x1FF;
    path[2] = (n >> (9 * 1)) & 0x1FF;
    path[1] = (n >> (9 * 2)) & 0x1FF;
    path[0] = (n >> (9 * 3)) & 0x1FF;

    return n;
}

// Returns present mask for new page
static __always_inline int path_inc(unsigned *path, pte_t **ptes)
{
    uintptr_t n = path_inc(path);

    ptes[3] = PT3_PTR + (n >> (9 * 0));
    ptes[2] = PT2_PTR + (n >> (9 * 1));
    ptes[1] = PT1_PTR + (n >> (9 * 2));
    ptes[0] = PT0_PTR + (n >> (9 * 3));

    return ptes_present(ptes);
}

static void mmu_mem_map_swap(physmem_range_t *a, physmem_range_t *b)
{
    physmem_range_t temp = *a;
    *a = *b;
    *b = temp;
}

__used
static int ptes_leaf_level(pte_t const **ptes)
{
    pte_t constexpr present_large = PTE_PRESENT | PTE_PAGESIZE;

    if (unlikely((*ptes[0] & PTE_PRESENT) == 0))
        return -1;

    for (int level = 1; level < 3; ++level) {
        pte_t const pte = *ptes[level];

        if (unlikely((pte & PTE_PRESENT) == 0))
            return -1;

        if (unlikely((pte & present_large) == present_large))
            return level;
    }

    return 3;
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

static pte_t *mm_map_aliasing_pte(pte_t *aliasing_pte, physaddr_t addr)
{
    linaddr_t linaddr;

    if ((linaddr_t)aliasing_pte >= PT_MAX_ADDR) {
        uintptr_t pt3_index = aliasing_pte - PT3_PTR;
        linaddr = (pt3_index << PAGE_SIZE_BIT);
        linaddr = CANONICALIZE(linaddr);
    } else {
        linaddr = 0xFFFFFFFF80000000 - PAGE_SIZE;
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
// Zero initialization

struct clear_phys_state_t {
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;
    lock_type locks[64];

    pte_t *pte;
    int log2_window_sz;
    int level;

    // 2TB before end of address space
    static constexpr linaddr_t addr = 0xFFFFFE0000000000;

    void clear(physaddr_t addr);
};

static clear_phys_state_t clear_phys_state;

void mm_phys_clear_init()
{
    if (cpuid_has_1gpage()) {
        clear_phys_state.log2_window_sz = 30;
        clear_phys_state.level = 1;
        printdbg("Using %s page(s) to clear page frames\n", "1GB");
    } else if (cpuid_has_2mpage()) {
        clear_phys_state.log2_window_sz = 21;
        clear_phys_state.level = 2;
        printdbg("Using %s page(s) to clear page frames\n", "2MB");
    } else {
        clear_phys_state.log2_window_sz = 12;
        clear_phys_state.level = 3;
        printdbg("Using %s page(s) to clear page frames\n", "4KB");
    }

    pte_t *ptes[4];
    ptes_from_addr(ptes, clear_phys_state_t::addr);

    pte_t global_mask = ~PTE_GLOBAL;

    pte_t *page_base = nullptr;
    for (int i = 0; i < clear_phys_state.level; ) {
        assert(*ptes[i] == 0);
        physaddr_t page = init_take_page(0);
        printdbg("Assigning phys_clear_init table pte=%p page=0x%lx\n",
                 (void*)ptes[i], page);
        *ptes[i] = (page | PTE_PRESENT | PTE_WRITABLE |
                PTE_ACCESSED | PTE_DIRTY | PTE_GLOBAL) & global_mask;

        // Clear new page table page
        cpu_invalidate_page(uintptr_t(ptes[++i]));
        clear_phys_state.pte = ptes[i];
        page_base = (pte_t*)(uintptr_t(ptes[i]) & -PAGE_SIZE);
        printdbg("page_base=0x%p\n", (void*)page_base);
        memset(page_base, 0, PAGE_SIZE);

        global_mask = -1;
    }
}

void clear_phys(physaddr_t addr)
{
    unsigned index = 0;
    pte_t& pte = clear_phys_state.pte[index << 3];

    pte_t page_flags;
    size_t offset;
    physaddr_t base;
    if (clear_phys_state.log2_window_sz == 30) {
        base = addr & -(1 << 30);
        page_flags = PTE_PAGESIZE;
    } else if (clear_phys_state.log2_window_sz == 21) {
        base = addr & -(1 << 21);
        page_flags = PTE_PAGESIZE;
    } else if (clear_phys_state.log2_window_sz == 12) {
        base = addr & -(1 << 12);
        page_flags = 0;
    } else {
        cpu_debug_break();
        __builtin_unreachable();
    }

    offset = addr - base;

    clear_phys_state_t::scoped_lock lock(clear_phys_state.locks[index]);

    linaddr_t window = clear_phys_state_t::addr +
            (index << (3 + clear_phys_state.log2_window_sz));

    pte_t expect = base | page_flags | PTE_WRITABLE | PTE_GLOBAL |
            PTE_ACCESSED | PTE_DIRTY | PTE_PRESENT;

    if (pte != expect)
        pte = expect;

    cpu_invalidate_page(window);

    clear64((char*)window + offset, PAGE_SIZE);
}

//
// Page table creation

static void mmu_map_page(linaddr_t addr, physaddr_t physaddr, pte_t flags)
{
    pte_t *ptes[4];
    pte_t pte;

    ptes_from_addr(ptes, addr);
    int present_mask = ptes_present(ptes);

    // G is MBZ in PML4e on AMD
    pte_t global_mask = ~PTE_GLOBAL;

    if (unlikely((present_mask & 0x07) != 0x07)) {
        pte_t path_flags = PTE_PRESENT | PTE_WRITABLE |
                (addr <= 0x7FFFFFFFFFFF ? PTE_USER : 0);

        for (int i = 0; i < 3; ++i) {
            pte = *ptes[i];

            if (!pte) {
                physaddr_t ptaddr = init_take_page(0);
                clear_phys(ptaddr);
                *ptes[i] = (ptaddr | path_flags) & global_mask;
            }
        }

#if GLOBAL_RECURSIVE_MAPPING
        global_mask = -1;
#endif
    }

    *ptes[3] = (physaddr & PTE_ADDR) | flags;
}

static void mmu_tlb_perform_shootdown(void)
{
    cpu_flush_tlb();
    thread_shootdown_notify();
}

// TLB shootdown IPI
static isr_context_t *mmu_tlb_shootdown_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_TLB_SHOOTDOWN);

    apic_eoi(intr);

    int cpu_number = thread_cpu_number();

    // Clear pending
    atomic_and(&shootdown_pending, ~(1 << cpu_number));

    mmu_tlb_perform_shootdown();

    return ctx;
}

static void mmu_send_tlb_shootdown(bool synchronous = false)
{
    int cpu_count = thread_cpu_count();
    if (unlikely(cpu_count <= 1))
        return;

    cpu_scoped_irq_disable irq_was_enabled;
    int cur_cpu = thread_cpu_number();
    int all_cpu_mask = (1 << cpu_count) - 1;
    int cur_cpu_mask = 1 << cur_cpu;
    int other_cpu_mask = all_cpu_mask & ~cur_cpu_mask;
    int old_pending = atomic_or(&shootdown_pending, other_cpu_mask);
    int need_ipi_mask = old_pending & other_cpu_mask;

    vector<uint64_t> shootdown_counts;
    if (synchronous) {
        shootdown_counts.reserve(thread_cpu_count());
        for (int i = 0; i < cpu_count; ++i) {
            shootdown_counts.push_back((i != cur_cpu)
                                       ? thread_shootdown_count(i)
                                       : -1);
        }
    }

    if (other_cpu_mask) {
        if (need_ipi_mask == other_cpu_mask) {
            // Send to all other CPUs
            apic_send_ipi(-1, INTR_TLB_SHOOTDOWN);
        } else {
            int cpu_mask = 1;
            for (int cpu = 0; cpu < cpu_count; ++cpu, cpu_mask <<= 1) {
                if (!(old_pending & cpu_mask) && (need_ipi_mask & cpu_mask))
                    thread_send_ipi(cpu, INTR_TLB_SHOOTDOWN);
            }
        }
    }

    if (unlikely(synchronous)) {
        uint64_t wait_st = nano_time();
        uint64_t loops = 0;
        for (int wait_count = cpu_count - 1; wait_count > 0; pause()) {
            for (int i = 0; i < cpu_count; ++i) {
                uint64_t &count = shootdown_counts[i];
                if (count != uint64_t(-1) &&
                        thread_shootdown_count(i) > count) {
                    count = -1;
                    --wait_count;
                }
            }
            ++loops;
        }
        uint64_t wait_en = nano_time();

        printdbg("TLB shootdown waited for %lu loops, %lu cycles\n", loops,
                 wait_en - wait_st);
    }
}

static isr_context_t *mmu_lazy_tlb_shootdown(isr_context_t *ctx)
{
    thread_set_cpu_mmu_seq(mmu_seq);
    cpu_flush_tlb();

    // Restart instruction
    return ctx;
}

static intptr_t mmu_device_from_addr(linaddr_t rounded_addr)
{
    mm_dev_mapping_scoped_lock lock(mm_dev_mapping_lock);

    intptr_t device = binary_search(
                mm_dev_mappings.data(), mm_dev_mappings.size(),
                sizeof(*mm_dev_mappings.data()),
                (void*)rounded_addr,
                mm_dev_map_search, 0, 1);

    return device;
}

// Page fault
isr_context_t *mmu_page_fault_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_EX_PAGE);

    atomic_inc(&page_fault_count);

    uintptr_t fault_addr = cpu_get_fault_address();

    pte_t *ptes[4];

    ptes_from_addr(ptes, fault_addr);
    int present_mask = ptes_present(ptes);

    // Examine the error code
    if (unlikely(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_R)) {
        // Reserved bit violation?!
        mmu_dump_pf(ISR_CTX_ERRCODE(ctx));
        mmu_dump_ptes(ptes);
        cpu_debug_break();
        cpu_invalidate_page(fault_addr);
        return nullptr;
    }

#if DEBUG_PAGE_FAULT
    printdbg("Page fault at %lx\n", fault_addr);
#endif

    pte_t pte = (present_mask >= 0x07) ? *ptes[3] : 0;

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

            if (ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_W)
                page_flags = PTE_PRESENT | PTE_ACCESSED | PTE_DIRTY;
            else
                page_flags = PTE_PRESENT | PTE_ACCESSED;

            // Update PTE and restart instruction
            if (atomic_cmpxchg(ptes[3], pte,
                               (pte & ~PTE_ADDR) |
                               (page & PTE_ADDR) |
                               page_flags) != pte) {
                // Another thread beat us to it
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
            intptr_t device = mmu_device_from_addr(rounded_addr);
            if (unlikely(device < 0))
                return nullptr;

            mmap_device_mapping_t *mapping = mm_dev_mappings[device];

            uint64_t mapping_offset = (char*)rounded_addr -
                    (char*)mapping->base_addr;

            // Round down to nearest 64KB boundary
            mapping_offset &= -0x10000;

            rounded_addr = linaddr_t(mapping->base_addr) + mapping_offset;

            pte_t volatile *vpte = ptes[3];

            // Attempt to be the first CPU to start reading a block
            unique_lock<mutex> lock(mapping->lock);
            while (mapping->active_read >= 0 &&
                   !(*vpte & PTE_PRESENT))
                mapping->done_cond.wait(lock);

            // If the page became present while waiting, then done
            if (*vpte & PTE_PRESENT)
                return ctx;

            // Become the reader for this mapping
            mapping->active_read = mapping_offset;
            lock.unlock();

            int io_result = mapping->callback(
                        mapping->context, (void*)rounded_addr,
                        mapping_offset, 0x10000, true, false);

            if (likely(io_result >= 0)) {
                // Mark the range present from end to start
                ptes_from_addr(ptes, rounded_addr);
                for (size_t i = (0x10000 >> PAGE_SIZE_BIT); i > 0; --i)
                    atomic_or(ptes[3] + (i - 1), PTE_PRESENT | PTE_ACCESSED);
            }

            lock.lock();
            mapping->active_read = -1;
            mapping->done_cond.notify_all();
            lock.unlock();

            // Restart the instruction, or unhandled exception on I/O error
            return likely(io_result >= 0) ? ctx : 0;
        } else if (pte & PTE_EX_WAIT) {
            // Must wait for another CPU to finish doing something with PTE
            cpu_wait_bit_clear(ptes[3], PTE_EX_WAIT_BIT);
            return ctx;
        } else {
            printdbg("Invalid page fault at 0x%zx, RIP=%p\n",
                     fault_addr, (void*)ISR_CTX_REG_RIP(ctx));
            if (thread_get_exception_top())
                return nullptr;

            dump_context(ctx, 1);

            assert(!"Invalid page fault");
        }
    } else if (present_mask != 0x0F) {
        if (thread_get_exception_top())
            return nullptr;

        dump_context(ctx, 1);

        assert(!"Invalid page fault path");
    }

    printdbg("#PF: present=%d\n"
             "     write=%d\n"
             "     user=%d\n"
             "     reserved bit violation=%d\n"
             "     instruction fetch=%d\n"
             "     protection key violation=%d\n"
             "     SGX violation=%d\n",
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_P),
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_W),
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_U),
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_R),
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_I),
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_PK),
             !!(ISR_CTX_ERRCODE(ctx) & CTX_ERRCODE_PF_SGX));

    printdbg("     present_mask=0x%x\n",
             present_mask);

    mmu_dump_pf(ISR_CTX_ERRCODE(ctx));
    mmu_dump_ptes(ptes);

    return nullptr;
}

#if DEBUG_ADDR_ALLOC
template<typename K, typename V>
static int dump_addr_node(typename rbtree_t<K,V>::kvp_t *kvp, void *p)
{
    char const **names = (char const **)p;
    printdbg("%s=%16lx %s=%16lx\n",
             names[0], kvp->key, names[1], kvp->val);
    return 0;
}

template<typename K, typename V>
static void dump_addr_tree(rbtree_t<K,V> *tree,
                           char const *key_name, char const *val_name)
{
    char const *names[] = {
        key_name,
        val_name
    };

    tree->walk(dump_addr_node<K,V>, names);
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
        cpu_msr_set(CPU_MSR_IA32_PAT, PAT_CFG);
}

void mmu_init()
{
    // Hook IPI for TLB shootdown
    intr_hook(INTR_TLB_SHOOTDOWN, mmu_tlb_shootdown_handler);

    memcpy(phys_mem_map, kernel_params->phys_mem_table,
           sizeof(*phys_mem_map) * kernel_params->phys_mem_table_size);
    phys_mem_map_count = kernel_params->phys_mem_table_size;

    usable_mem_ranges = mmu_fixup_mem_map(phys_mem_map);

    if (usable_mem_ranges > countof(mem_ranges)) {
        printdbg("Physical memory is incredibly fragmented!\n");
        usable_mem_ranges = countof(mem_ranges);
    }

    for (physmem_range_t *ranges_in = phys_mem_map, *ranges_out = mem_ranges;
         ranges_in < phys_mem_map + phys_mem_map_count; ++ranges_in) {
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

    physaddr_t root_phys_addr;
    pte_t *pt;

    // Create the new root

    // Get a page
    root_phys_addr = init_take_page(0);
    assert(root_phys_addr != 0);

    pte_t *old_root = (pte_t*)(cpu_get_page_directory() & PTE_ADDR);

    pt = init_map_aliasing_pte(aliasing_pte, root_phys_addr);
    memcpy(pt, old_root, PAGESIZE);

    pte_t ptflags = PTE_PRESENT | PTE_WRITABLE;

    pt = init_map_aliasing_pte(aliasing_pte, root_phys_addr);
    assert(pt[PT_RECURSE] == 0);
    pt[PT_RECURSE] = root_phys_addr | ptflags;

    root_physaddr = root_phys_addr;

    mmu_configure_pat();

    //pt = init_map_aliasing_pte(aliasing_pte, root_physaddr);
    cpu_set_page_directory(root_phys_addr);


    // Make zero page not present to catch null pointers
    *PT3_PTR = 0;
    cpu_invalidate_page(0);
    cpu_flush_tlb();

    mm_phys_clear_init();

    size_t physalloc_size = mmu_phys_allocator_t::size_from_highest_page(
                highest_usable);
    void *phys_alloc = mmap(0, physalloc_size,
                            PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    printdbg("Building physical memory free list\n");

    // Get the highest address taken by the kernel
    physaddr_t top_of_kernel = ___top_physaddr;

    // Reserve 4MB low memory for contiguous
    // physical allocator
    physaddr_t contiguous_start = top_of_kernel;
    top_of_kernel += 4 << 20;

    phys_allocator.init(phys_alloc, 0x100000);

    // Put all of the remaining physical memory into the free lists
    uintptr_t free_count = 0;

    while (usable_mem_ranges) {
        physmem_range_t range = mem_ranges[usable_mem_ranges-1];
        if (range.base < top_of_kernel) {
            size_t removed = top_of_kernel - range.base;
            range.base += removed;
            range.size -= removed;
        }

        // There's no way a range spans the 1MB line
        assert((range.base <= 0x100000) ==
               ((range.base + range.size) <= 0x100000));

        phys_allocator.add_free_space(range.base, range.size);
        --usable_mem_ranges;

        if (range.base == top_of_kernel) {
            usable_mem_ranges = 0;
            break;
        }
    }

    // Start using physical memory allocator
    usable_mem_ranges = 0;

    printdbg("%lu pages free (%luMB)\n",
           free_count,
           free_count >> (20 - PAGE_SIZE_BIT));

    intr_hook(INTR_EX_PAGE, mmu_page_fault_handler);

    malloc_startup(nullptr);

    // Prepare 4MB contiguous physical memory
    // allocator with a capacity of 128
    contig_phys_allocator.early_init(&contiguous_start, 4 << 20,
                                     "contig_phys_allocator");

    linear_allocator.early_init((linaddr_t*)&linear_base,
                                clear_phys_state_t::addr - linear_base,
                                "linear_allocator");

    near_allocator.early_init((linaddr_t*)&near_base, 0ULL - near_base,
                              "near_allocator");

    // Allocate guard page
    linear_allocator.alloc_linear(PAGE_SIZE);

    // Preallocate the second level kernel PTPD pages so we don't
    // need to worry about process-specific page directory synchronization
    for (size_t i = 256; i < 512; ++i) {
        if (PT0_PTR[i] == 0) {
            physaddr_t page = mmu_alloc_phys(0);
            clear_phys(page);
            // Global bit MBZ in PML4e on AMD
            PT0_PTR[i] = page | PTE_WRITABLE | PTE_PRESENT;
        }
    }

    // Alias master page directory to be accessible
    // from all process contexts
    master_pagedir = (pte_t*)mmap((void*)root_physaddr, PAGE_SIZE, PROT_READ,
                          MAP_PHYSICAL, -1, 0);

    current_pagedir = (pte_t*)(PT0_PTR);

    // Find holes in address space suitable for mapping hardware
    bool first_hole = true;

    physaddr_t last_hole_end = 0x100000;
    for (size_t i = 0; i < phys_mem_map_count; ++i) {
        physmem_range_t const& range = phys_mem_map[i];

        // Ignore holes in 1st MB
        if (range.base < 0x100000)
            continue;

        if (range.base > last_hole_end) {
            if (!first_hole) {
                hole_allocator.release_linear(last_hole_end,
                                              range.base - last_hole_end);
            } else {
                first_hole = false;
                hole_allocator.init(last_hole_end, range.base - last_hole_end,
                                    "hole_allocator");
            }
        }

        last_hole_end = range.base + range.size;
    }

    physaddr_t max_physaddr = physaddr_t(1) << cpuid_paddr_bits();

    if (max_physaddr > last_hole_end) {
        if (!first_hole) {
            hole_allocator.release_linear(last_hole_end,
                                          max_physaddr - last_hole_end);
        } else {
            first_hole = false;
            hole_allocator.init(last_hole_end,
                                max_physaddr - last_hole_end,
                                "hole_allocator");
        }
    }

    callout_call(callout_type_t::vmm_ready);
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

void contiguous_allocator_t::early_init(linaddr_t *addr, size_t size,
                                        char const *name)
{
    this->name = name;

    linaddr_t initial_addr = *addr;

    free_addr_by_addr.init(contiguous_allocator_cmp_key, 0);
    free_addr_by_size.init(contiguous_allocator_cmp_both, 0);

    // Account for space taken creating trees

    size_t size_adj = *addr - initial_addr;
    size -= size_adj;

    free_addr_by_size.insert(size, *addr);
    free_addr_by_addr.insert(*addr, size);

    dump("After early_init\n");
}

void contiguous_allocator_t::init(linaddr_t addr, size_t size,
                                  char const *name)
{
    this->name = name;

    free_addr_by_addr.init(contiguous_allocator_cmp_key, 0);
    free_addr_by_size.init(contiguous_allocator_cmp_both, 0);

    if (size) {
        free_addr_by_size.insert(size, addr);
        free_addr_by_addr.insert(addr, size);
    }

    dump("After init\n");
}

uintptr_t contiguous_allocator_t::alloc_linear(size_t size)
{
    linaddr_t addr;

    if (likely(free_addr_by_addr && free_addr_by_size)) {
        scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
        dump("Before Alloc %lx\n", size);
#endif

        // Find the lowest address item that is big enough
        tree_t::iter_t place = free_addr_by_size.lower_bound(size, 0);

        tree_t::kvp_t by_size = free_addr_by_size.item(place);

        if (by_size.key < size) {
            place = free_addr_by_size.next(place);
            by_size = free_addr_by_size.item(place);
        }

        free_addr_by_size.delete_at(place);

        // Delete corresponding entry by address
        free_addr_by_addr.delete_item(by_size.val, by_size.key);

        if (by_size.key > size) {
            // Insert remainder by size
            free_addr_by_size.insert(by_size.key - size, by_size.val + size);

            // Insert remainder by address
            free_addr_by_addr.insert(by_size.val + size, by_size.key - size);
        }

        addr = by_size.val;

#if DEBUG_ADDR_ALLOC
        dump("after alloc_linear sz=%zx addr=%zx\n", size, addr);
#endif

#if DEBUG_LINEAR_SANITY
        sanity_check_by_size(free_addr_by_size);
        sanity_check_by_addr(free_addr_by_addr);
#endif

#if DEBUG_ADDR_ALLOC
        printdbg("Took address space @ %lx,"
                 " size=%lx\n", addr, size);
#endif
    } else {
        addr = atomic_xadd(&linear_base, size);

        printdbg("Took early address space @ %lx,"
                 " size=%lx,"
                 " new linear_base=%lx\n", addr, size, linear_base);
    }

    return addr;
}

bool contiguous_allocator_t::take_linear(linaddr_t addr, size_t size,
                                         bool require_free)
{
    assert(free_addr_by_addr);
    assert(free_addr_by_size);

    scoped_lock lock(free_addr_lock);

    // Round to pages
    addr &= -PAGE_SIZE;
    size = round_up(size);

    linaddr_t end = addr + size;

    // Find the last free range before or at the address
    tree_t::iter_t place = free_addr_by_addr.lower_bound(addr, 0);

    tree_t::iter_t next_place;

    for (; place; place = next_place) {
        tree_t::kvp_t by_addr = free_addr_by_addr.item(place);

        if (by_addr.key <= addr && (by_addr.key + by_addr.val) >= end) {
            //
            // Need to punch a hole in the middle of the free block

            // Delete the size entry
            free_addr_by_size.delete_item(by_addr.val, by_addr.key);

            // Delete the address entry
            free_addr_by_addr.delete_at(place);

            // Free space up to beginning of hole
            tree_t::kvp_t new_before = {
                by_addr.key,
                addr - by_addr.key
            };

            // Free space after end of hole
            tree_t::kvp_t new_after = {
                end,
                (by_addr.key + by_addr.val) - end
            };

            if (new_before.val > 0)
                free_addr_by_addr.insert_pair(&new_before);

            if (new_after.val > 0)
                free_addr_by_addr.insert_pair(&new_after);

            if (new_before.val > 0)
                free_addr_by_size.insert(new_before.val, new_before.key);

            if (new_after.val > 0)
                free_addr_by_size.insert(new_after.val, new_after.key);

            return true;
        } else if (!require_free) {
            return false;
        } else if (by_addr.key < addr && by_addr.key + by_addr.val > addr) {
            //
            // The found free block is before the range and overlaps it

            // Save next block
            next_place = free_addr_by_addr.next(place);

            // Delete the size entry
            free_addr_by_size.delete_item(by_addr.val, by_addr.key);

            // Delete the address entry
            free_addr_by_addr.delete_at(place);

            // Create a smaller block that does not overlap taken range
            by_addr.val = addr - by_addr.key;

            // Insert smaller range by address
            free_addr_by_addr.insert_pair(&by_addr);

            // Insert smaller range by size
            free_addr_by_size.insert(by_addr.val, by_addr.key);
        } else if (by_addr.key >= addr && by_addr.key + by_addr.val <= end) {
            //
            // Range completely covers block, delete block

            next_place = free_addr_by_addr.next(place);

            free_addr_by_size.delete_item(
                        by_addr.val, by_addr.key);

            free_addr_by_addr.delete_at(place);
        } else if (by_addr.key > addr) {
            //
            // Range cut off some of beginning of block

            free_addr_by_size.delete_item(by_addr.val, by_addr.key);

            free_addr_by_addr.delete_at(place);

            return true;
        } else {
            assert(by_addr.key >= end);

            // Block is past end of allocated range, done
            return true;
        }
    }

    return false;
}

void contiguous_allocator_t::release_linear(uintptr_t addr, size_t size)
{
    // Round address down to page boundary
    size_t misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;

    // Round size up to multiple of page size
    size = round_up(size);

    linaddr_t end = addr + size;

    scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
    dump("---- Free %lx @ %lx\n", size, addr);
#endif

    // Find the nearest free block before the freed range
    tree_t::iter_t pred_it = free_addr_by_addr.lower_bound(addr, 0);

    // Find the nearest free block after the freed range
    tree_t::iter_t succ_it = free_addr_by_addr.lower_bound(end, ~0UL);

    tree_t::kvp_t pred = free_addr_by_addr.item(pred_it);
    tree_t::kvp_t succ = free_addr_by_addr.item(succ_it);

    int coalesce_pred = ((pred.key + pred.val) == addr);
    int coalesce_succ = (succ.key == end);

    if (coalesce_pred) {
        addr -= pred.val;
        size += pred.val;
        free_addr_by_addr.delete_at(pred_it);

        free_addr_by_size.delete_item(pred.val, pred.key);
    }

    if (coalesce_succ) {
        size += succ.val;
        free_addr_by_addr.delete_at(succ_it);

        free_addr_by_size.delete_item(succ.val, succ.key);
    }

    free_addr_by_size.insert(size, addr);
    free_addr_by_addr.insert(addr, size);

#if DEBUG_ADDR_ALLOC
    dump("Addr map by addr (after free)");
#endif
}

void contiguous_allocator_t::dump(const char *format, ...)
{
#if DEBUG_ADDR_ALLOC
    va_list ap;
    va_start(ap, format);
    vprintdbg(format, ap);
    va_end(ap);

    printdbg("By addr\n");
    dump_addr_tree(&free_addr_by_addr, "addr", "size");
    printdbg("By size\n");
    dump_addr_tree(&free_addr_by_size, "size", "addr");
#endif
}

// Returns the present mask for the new page
static __always_inline int ptes_step(pte_t **ptes)
{
    ++ptes[3];
    if (unlikely(!(uintptr_t(ptes[3]) & 0xFF8))) {
        // Carry
        ++ptes[2];
        if (unlikely(!(uintptr_t(ptes[2]) & 0xFF8))) {
            // Carry
            ++ptes[1];
            if (unlikely(!(uintptr_t(ptes[1]) & 0xFF8))) {
                // Carry
                ++ptes[0];
            }
        }
    }
    return ptes_present(ptes);
}

// Returns number of last level PTEs to skip forward
// to get to next entry at the lowest level that is not present
// Makes sense when we are at a 4KB or 2MB or 1GB or 512GB boundary
static __always_inline ptrdiff_t ptes_mask_skip(int present_mask)
{
    // See if we need to advance 512GB
    if (unlikely(!(present_mask & 1)))
        return ptrdiff_t(512) * 512 * 512;

    // See if we need to advance 1GB
    if (unlikely(!(present_mask & 2)))
        return ptrdiff_t(512) * 512;

    // See if we need to advance 2MB
    if (unlikely(!(present_mask & 4)))
        return 512;

    // No skip
    return 0;
}

// Returns true at 2MB boundaries,
// which indicates that the present mask needs to be checked
static __always_inline bool ptes_advance(pte_t **ptes, ptrdiff_t distance)
{
    size_t page_index = (ptes[3] - PT3_PTR) + distance;
    ptes[3] += distance;
    ptes[2] = PT2_PTR + (page_index >> (9 * 1));
    ptes[1] = PT1_PTR + (page_index >> (9 * 2));
    ptes[0] = PT0_PTR + (page_index >> (9 * 3));
    return (page_index & 0x1FF) == 0;
}

//
// Public API

bool mpresent(uintptr_t addr, size_t size)
{
    pte_t *ptes[4];

    unsigned misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;
    size = round_up(size);

    ptes_from_addr(ptes, addr);
    pte_t *end = ptes[3] + (size >> PAGE_SCALE);

    while (ptes[3] < end) {
        if (ptes_present(ptes) != 0x0F)
            return false;
        ptes_step(ptes);
    }

    return true;
}

bool mwritable(uintptr_t addr, size_t size)
{

    unsigned misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;
    size = round_up(size);

    pte_t *ptes[4];
    ptes_from_addr(ptes, addr);
    pte_t *end = ptes[3] + (size >> PAGE_SCALE);

    while (ptes[3] < end) {
        if (ptes_present(ptes) != 0x0F)
            return false;

        if (!(*ptes[3] & PTE_WRITABLE))
            return false;

        ptes_step(ptes);
    }

    return true;
}

static pte_t *mm_create_pagetables_aligned(uintptr_t start, size_t size)
{
    pte_t *pte_st[4];
    pte_t *pte_en[4];

    uintptr_t const end = start + size - 1;

    ptes_from_addr(pte_st, start);
    ptes_from_addr(pte_en, end);

    // Fastpath small allocations fully within a 2MB region,
    // where there is nothing to do
    if (pte_st[2] == pte_en[2] &&
            (*pte_st[0] & PTE_PRESENT) &&
            (*pte_st[1] & PTE_PRESENT) &&
            (*pte_st[2] & PTE_PRESENT))
        return pte_st[3];

    bool const low = (start & 0xFFFFFFFFFFFFU) < 0x800000000000U;

    // Mark high pages PTE_GLOBAL, low pages PTE_USER
    // First 3 levels PTE_ACCESSED and PTE_DIRTY
    pte_t const page_flags = (low ? PTE_USER : PTE_GLOBAL) |
            PTE_ACCESSED | PTE_DIRTY | PTE_PRESENT | PTE_WRITABLE;

    pte_t global_mask = ~PTE_GLOBAL;

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    for (unsigned level = 0; level < 3; ++level) {
        pte_t * const base = pte_st[level];
        size_t const range_count = (pte_en[level] - base) + 1;
        for (size_t i = 0; i < range_count; ++i) {
            pte_t old = base[i];
            if (!(old & PTE_PRESENT)) {
                physaddr_t const page = mmu_alloc_phys(0);

                clear_phys(page);

                pte_t new_pte = (page | page_flags) & global_mask;

                if (atomic_cmpxchg(base + i, old, new_pte) != old)
                    free_batch.free(page);
            }
        }

#if GLOBAL_RECURSIVE_MAPPING
        global_mask = -1;
#endif
    }

    return pte_st[3];
}

template<typename T>
static __always_inline T zero_if_false(bool cond, T bits)
{
    return bits & -cond;
}

template<typename T>
static __always_inline T select_mask(bool cond, T true_val, T false_val)
{
    T mask = -cond;
    return (true_val & mask) | (false_val & ~mask);
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    (void)offset;
    assert(offset == 0);

    // Fail on invalid protection mask
    if (unlikely(prot != (prot & (PROT_READ | PROT_WRITE | PROT_EXEC))))
        return MAP_FAILED;

    // Any invalid flag set returns failure
    if (unlikely(flags & MAP_INVALID_MASK))
        return MAP_FAILED;

    static constexpr int user_prohibited =
            MAP_NEAR | MAP_DEVICE | MAP_GLOBAL |
            MAP_UNINITIALIZED | MAP_WEAKORDER |
            MAP_NOCACHE | MAP_WRITETHRU;

    if (unlikely((flags & MAP_USER) && (flags & user_prohibited)))
        return MAP_FAILED;

    if (len == 0)
        return nullptr;

    PROFILE_MMAP_ONLY( uint64_t profile_st = cpu_rdtsc() );

    // Must pass MAP_DEVICE if passing a device registration index
    assert((flags & MAP_DEVICE) || (fd < 0));

#if DEBUG_PAGE_TABLES
    printdbg("Mapping len=%zx prot=%x flags=%x addr=%lx\n",
             len, prot, flags, (uintptr_t)addr);
#endif

    pte_t page_flags = 0;

    // Populate 32 bit memory, always
    flags |= zero_if_false(flags & MAP_32BIT, MAP_POPULATE);

    // Populate kernel stacks
    flags |= zero_if_false((flags & (MAP_STACK | MAP_USER)) == MAP_STACK,
                           MAP_POPULATE);

    // Set physical and present flag on physical memory mapping
    page_flags |= zero_if_false(flags & MAP_PHYSICAL,
                                PTE_EX_PHYSICAL | PTE_PRESENT);

    if (unlikely(flags & MAP_WEAKORDER)) {
        // Weakly ordered memory, set write combining in PTE if capable
        page_flags |= select_mask(mmu_have_pat(),
                                  PTE_PTEPAT_n(PAT_IDX_WC),
                                  PTE_PCD | PTE_PWT);
    } else {
        page_flags |= zero_if_false(flags & MAP_NOCACHE, PTE_PCD);
        page_flags |= zero_if_false(flags & MAP_WRITETHRU, PTE_PWT);
    }

    page_flags |= zero_if_false(flags & MAP_POPULATE, PTE_PRESENT);
    page_flags |= zero_if_false(flags & MAP_DEVICE, PTE_EX_DEVICE);
    page_flags |= zero_if_false(flags & MAP_USER, PTE_USER);
    page_flags |= zero_if_false(!(flags & MAP_USER), PTE_GLOBAL);
    page_flags |= zero_if_false(prot & PROT_WRITE, PTE_WRITABLE);
    page_flags |= zero_if_false(!(prot & PROT_EXEC), PTE_NX & cpuid_nx_mask);

    uintptr_t misalignment = uintptr_t(addr) & PAGE_MASK;
    addr = (void*)(uintptr_t(addr) - misalignment);
    len += misalignment;
    len = round_up(len);

    contiguous_allocator_t *allocator =
            (flags & MAP_USER) ?
                (contiguous_allocator_t*)
                thread_current_process()->get_allocator()
            : (flags & MAP_NEAR) ? &near_allocator
            : &linear_allocator;

    PROFILE_LINEAR_ALLOC_ONLY( uint64_t profile_linear_st = cpu_rdtsc() );
    linaddr_t linear_addr;
    if (!addr || (flags & MAP_PHYSICAL)) {
        linear_addr = allocator->alloc_linear(len);
    } else {
        linear_addr = (linaddr_t)addr;

        if (unlikely((flags & MAP_USER) &&
                     (linear_addr >= 0x7FFFFFFFF000 ||
                      linear_addr + len > 0x7FFFFFFFF000)))
            return MAP_FAILED;

        if (unlikely(!allocator->take_linear(
                         linear_addr, len, flags & MAP_EXCLUSIVE)))
            return MAP_FAILED;
    }

    PROFILE_LINEAR_ALLOC_ONLY( printdbg("Allocation of %lu bytes of"
                                        " address space took %lu cycles\n",
                                        len, cpu_rdtsc() - profile_linear_st) );

    assert(linear_addr > 0x100000);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    if (likely(!usable_mem_ranges)) {
        pte_t *base_pte = mm_create_pagetables_aligned(linear_addr, len);

        //printdbg("mmap %zx bytes at %zx-%zx\n",
        //         len, linear_addr, linear_addr + len);

        if ((flags & (MAP_POPULATE | MAP_PHYSICAL)) == MAP_POPULATE) {
            // POPULATE, not PHYSICAL

            bool low = !!(flags & MAP_32BIT);

            bool success;
            success = phys_allocator.alloc_multiple(
                        low, len, [&](size_t ofs, physaddr_t paddr) {
                if (likely(!(flags & MAP_UNINITIALIZED)))
                    clear_phys(paddr);

                pte_t old = atomic_xchg(base_pte + (ofs >> 12),
                                        pte_t(paddr | page_flags));

                if (old && ((old & PTE_ADDR) != PTE_ADDR))
                    free_batch.free(old & PTE_ADDR);

                return true;
            });

            if (unlikely(!success))
                return MAP_FAILED;
        } else if (!(flags & MAP_PHYSICAL)) {
            // Demand paged

            size_t ofs = 0;
            pte_t pte;

            physaddr_t paddr = 0;

            size_t end = len >> PAGE_SCALE;

            if (flags & MAP_NOCOMMIT) {
                paddr = PTE_ADDR;
            } else if (!(flags & MAP_DEVICE)) {
                paddr = mmu_alloc_phys(0);

                if (paddr && !(flags & MAP_UNINITIALIZED))
                    clear_phys(paddr);
            }

            pte = 0;

            if (paddr)
                pte = pte_t(paddr | page_flags | PTE_PRESENT);

            if (paddr && !(flags & MAP_STACK)) {
                // Commit first page
                pte = atomic_xchg(base_pte, pte);

                ++ofs;
            } else if (paddr) {
                // Commit last page

                pte = atomic_xchg(base_pte + --end, pte);
            }

            if (unlikely(pte && pte != PTE_ADDR))
                free_batch.free(pte & PTE_ADDR);

            for ( ; ofs < end; ++ofs) {
                pte = pte_t(PTE_ADDR | page_flags);
                pte = atomic_xchg(base_pte + ofs, pte);

                if (unlikely(pte && pte != PTE_ADDR))
                    free_batch.free(pte & PTE_ADDR);
            }
        } else if (flags & MAP_PHYSICAL) {
            pte_t pte;

            physaddr_t paddr = physaddr_t(addr);
            for (size_t ofs = 0, end = (len >> PAGE_SCALE); ofs < end;
                 ++ofs, paddr += PAGE_SIZE) {
                pte = pte_t(paddr | page_flags);
                pte = atomic_xchg(base_pte + ofs, pte);

                if (pte && pte != PTE_ADDR)
                    free_batch.free(pte & PTE_ADDR);
            }
        } else {
            assert(!"Unhandled condition");
        }
    } else {
        // Early
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
                    if (likely(!(flags & MAP_UNINITIALIZED)))
                        clear_phys(page);
                }

                if (linear_addr + ofs == 0xFFFF80C020200000)
                    printdbg("Mapping %lx\n", linear_addr + ofs);

                mmu_map_page(linear_addr + ofs, page, page_flags |
                             ((!ofs) & PTE_PRESENT));
            } else {
                // addr is a physical address, caller uses
                // returned linear address to access it
                mmu_map_page(linear_addr + ofs,
                             (((physaddr_t)addr) + ofs) & PTE_ADDR,
                             page_flags);
            }
        }
    }

    atomic_inc(&mmu_seq);

    assert(linear_addr > 0x100000);

    PROFILE_MMAP_ONLY( printdbg("mmap of %zd bytes took %ld cycles\n",
                                len, cpu_rdtsc() - profile_st); )

    return (void*)(linear_addr + misalignment);
}

void *mremap(
        void *old_address,
        size_t old_size,
        size_t new_size,
        int flags,
        void *new_address)
{
    old_size = round_up(old_size);
    new_size = round_up(new_size);

    // Convert pointer to address
    linaddr_t old_st = linaddr_t(old_address);
    linaddr_t new_st = linaddr_t(new_address);

    // Fail with EINVAL if:
    //  - old_address and new_address must be page aligned
    //  - Flags must be valid
    //  - New size must be nonzero
    //  - Old address must be sane
    //  - new_address is zero and MREMAP_FIXED was set in flags
    //  - new_address is nonzero and MREMAP_FIXED was not set in flags
    if (unlikely(old_st & PAGE_MASK) ||
            unlikely(old_st < 0x400000) ||
            unlikely(new_st & PAGE_MASK) ||
            unlikely(new_st && new_st < 0x400000) ||
            unlikely(old_size == 0) ||
            unlikely(new_size == 0) ||
            unlikely(flags & MREMAP_INVALID_MASK) ||
            unlikely((!(flags & MREMAP_FIXED)) == (new_st == 0))) {
        thread_set_error(errno_t::EINVAL);
        return MAP_FAILED;
    }

    // Only support simplified API:
    //  - Must pass MREMAP_MAYMOVE
    //  - Must not pass MREMAP_FIXED
    if (flags != MREMAP_MAYMOVE) {
        thread_set_error(errno_t::ENOSYS);
        return MAP_FAILED;
    }

    if (new_size < old_size) {
        //
        // Got smaller

        size_t freed_size = old_size - new_size;

        // Release space at the end
        munmap((void*)(old_st + new_size), freed_size);

        return (void*)old_st;
    } else if (unlikely(new_size == old_size)) {
        return (void*)new_st;
    }

    bool const low = (old_st < 0x800000000000U);
    contiguous_allocator_t *allocator = low ?
                (contiguous_allocator_t*)
                thread_current_process()->get_allocator() :
                &linear_allocator;

    pte_t *new_base;

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    if (allocator->take_linear(old_st + old_size, new_size - old_size, true)) {
        // Expand in place
        new_st = old_st + old_size;
        new_size -= old_size;
        new_base = mm_create_pagetables_aligned(new_st, new_size);
        pte_t new_pte = (low ? PTE_USER : PTE_GLOBAL) |
                PTE_WRITABLE | PTE_ADDR;
        for (size_t i = 0, e = new_size >> 3; i < e; ++i) {
            pte_t pte = atomic_xchg(new_base + i, new_pte);
            if (pte && (pte & PTE_ADDR) != PTE_ADDR)
                free_batch.free(pte & PTE_ADDR);
        }
        return (void*)old_st;
    }

    pte_t *old_pte[4];
    ptes_from_addr(old_pte, old_st);

    new_st = allocator->alloc_linear(new_size);
    new_base = mm_create_pagetables_aligned(new_st, new_size);

    for (size_t i = 0, e = old_size >> PAGE_SCALE; i < e; ++i) {
        pte_t pte = atomic_xchg(old_pte[3] + i, 0);
        pte = atomic_xchg(new_base + i, pte);
        if (pte && (pte & PTE_ADDR) != PTE_ADDR)
            free_batch.free(pte & PTE_ADDR);
    }

    return (void*)new_st;
}

int munmap(void *addr, size_t size)
{
    linaddr_t a = (linaddr_t)addr;

    uintptr_t misalignment = a & PAGE_MASK;
    a -= misalignment;
    size += misalignment;
    size = round_up(size);

    pte_t *ptes[4];
    ptes_from_addr(ptes, a);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    size_t freed = 0;
    int present_mask = ptes_present(ptes);
    for (size_t ofs = 0; ofs < size; ofs += PAGE_SIZE) {
        if ((present_mask & 0x07) == 0x07) {
            pte_t pte = atomic_xchg(ptes[3], 0);

            if (likely(!(pte & PTE_EX_PHYSICAL))) {
                physaddr_t physaddr = pte & PTE_ADDR;

                if (physaddr && (physaddr != PTE_ADDR))
                    free_batch.free(physaddr);
            }

            if (pte & PTE_PRESENT) {
                ++freed;
                cpu_invalidate_page(a);
            }
        }

        a += PAGE_SIZE;

        if (ptes_advance(ptes, 1))
            present_mask = ptes_present(ptes);
    }

    if (freed)
        mmu_send_tlb_shootdown();

    contiguous_allocator_t *allocator =
            (a < 0x800000000000U) ?
                (contiguous_allocator_t*)
                thread_current_process()->get_allocator()
            : (a >= 0xFFFFFFFF80000000) ? &near_allocator
            : &linear_allocator;

    allocator->release_linear((linaddr_t)addr - misalignment, size);

    return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
    // Fail on invalid protection mask
    if (unlikely(prot != (prot & (PROT_READ | PROT_WRITE | PROT_EXEC)))) {
        assert(!"Invalid parameter!");
        return -1;
    }

    // Fail on invalid address
    if (!addr)
        return -1;

    unsigned misalignment = (uintptr_t)addr & PAGE_MASK;
    addr = (char*)addr - misalignment;
    len += misalignment;

    len = round_up(len);

    if (unlikely(len == 0))
        return 0;

    /// Demand paged PTE, readable
    ///  present=0, addr=PTE_ADDR
    /// Demand paged PTE, not readable
    ///  present=0, addr=(PTE_ADDR>>1)&PTE_ADDR

    // Unreadable demand paged has MSB of physical address cleared
    pte_t const demand_no_read = (PTE_ADDR >> 1) & PTE_ADDR;

    pte_t no_exec = cpuid_nx_mask & PTE_NX;

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

    pte_t *pt[4];
    ptes_from_addr(pt, linaddr_t(addr));
    pte_t *end = pt[3] + (len >> PAGE_SCALE);

    while (pt[3] < end)
    {
        assert((*pt[0] & PTE_PRESENT) &&
                (*pt[1] & PTE_PRESENT) &&
                (*pt[2] & PTE_PRESENT));

        pte_t replace;
        for (pte_t expect = *pt[3]; ; pause()) {
            pte_t demand_paged = ((expect & demand_no_read) == demand_no_read);

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

        ptes_step(pt);
    }

    mmu_send_tlb_shootdown();

    return 1;
}

// Support discarding pages and reverting to demand
// paged state with MADV_DONTNEED.
// Support enabling/disabling write combining
// with MADV_WEAKORDER/MADV_STRONGORDER
int madvise(void *addr, size_t len, int advice)
{
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

    pte_t *pt[4];
    ptes_from_addr(pt, linaddr_t(addr));
    pte_t *end = pt[3] + (len >> PAGE_SCALE);

    pte_t const demand_mask = (PTE_ADDR >> 1) & PTE_ADDR;

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    while (pt[3] < end &&
           (*pt[0] & PTE_PRESENT) &&
           (*pt[1] & PTE_PRESENT) &&
           (*pt[2] & PTE_PRESENT))
    {
        pte_t replace;
        for (pte_t expect = *pt[3]; ; pause()) {
            if (order_bits == pte_t(-1)) {
                // Discarding
                physaddr_t page = 0;
                if (expect && (expect & demand_mask) != demand_mask) {
                    page = expect & PTE_ADDR;
                    replace = expect | PTE_ADDR;

                    if (unlikely(!atomic_cmpxchg_upd(pt[3], &expect, replace)))
                        continue;

                    free_batch.free(page);
                }
                break;
            } else {
                // Weak/Strong order
                replace = (expect & ~(PTE_PTEPAT | PTE_PCD | PTE_PWT)) |
                        order_bits;

                if (likely(atomic_cmpxchg_upd(pt[3], &expect, replace)))
                    break;
            }
        }

        cpu_invalidate_page((uintptr_t)addr);
        addr = (char*)addr + PAGE_SIZE;

        ptes_step(pt);
    }

    mmu_send_tlb_shootdown();

    return 0;
}

// Returns the return value of the callback
// Ends the loop when the callback returns a negative number
// Calls the callback with each present sub-range
// Callback signature: int(linaddr_t base, size_t len)
template<typename F>
static int present_ranges(F callback, linaddr_t rounded_addr, size_t len)
{
    assert(rounded_addr != 0);
    assert(((rounded_addr) & PAGE_MASK) == 0);
    assert(((len) & PAGE_MASK) == 0);

    pte_t *ptes[4];

    linaddr_t end = rounded_addr + len;

    ptes_from_addr(ptes, rounded_addr);
    int present_mask = ptes_present(ptes);

    int result = 0;
    linaddr_t range_start = 0;

    for (linaddr_t addr = rounded_addr;
         result >= 0 && addr < end; addr += PAGE_SIZE) {
        if (present_mask == 0xF) {
            if (!range_start)
                range_start = addr;
        } else {
            if (range_start)
                result = callback(range_start, addr - range_start);

            range_start = 0;
        }

        ptes_step(ptes);
        present_mask = ptes_present(ptes);
    }

    if (range_start && result >= 0)
        result = callback(range_start, end - range_start);

    return result;
}

int msync(void const *addr, size_t len, int flags)
{
    // Check for validity, particularly accidentally using O_SYNC
    assert((flags & (MS_SYNC | MS_INVALIDATE)) == flags);

    linaddr_t rounded_addr = linaddr_t(addr) & -intptr_t(PAGE_SIZE);
    len = round_up(len);

    if (unlikely(len == 0))
        return 0;

    intptr_t device = mmu_device_from_addr(rounded_addr);

    if (unlikely(device < 0))
        return -int(errno_t::EFAULT);

    mmap_device_mapping_t *mapping = mm_dev_mappings[device];

    unique_lock<mutex> lock(mapping->lock);

    while (mapping->active_read >= 0)
        mapping->done_cond.wait(lock);

    bool need_flush = (flags & MS_SYNC) != 0;

    int result = present_ranges([&](linaddr_t base, size_t range_len) -> int {
        uintptr_t offset = base - uintptr_t(mapping->base_addr);

        return mapping->callback(mapping->context, (void*)base,
                                 offset, range_len, false, need_flush);
    }, rounded_addr, len);

    return result;
}

uintptr_t mphysaddr(void volatile *addr)
{
    linaddr_t linaddr = linaddr_t(addr);

    unsigned misalignment = linaddr & PAGE_MASK;
    linaddr -= misalignment;

    pte_t *ptes[4];
    ptes_from_addr(ptes, linaddr);
    int present_mask = ptes_present(ptes);

    if ((present_mask & 0x07) != 0x07)
        return 0;

    pte_t pte = *ptes[3];
    physaddr_t page = pte & PTE_ADDR;

    // If page is being demand paged
    if (page == PTE_ADDR) {
        // Commit a page
        page = mmu_alloc_phys(0);

        clear_phys(page);

        pte_t new_pte = (pte & ~PTE_ADDR) | page;

        if (atomic_cmpxchg_upd(ptes[3], &pte, new_pte))
            pte = new_pte;
        else
            mmu_free_phys(page);
    } else if (!(pte & PTE_PRESENT)) {
        // Assert that it is not a PROT_NONE page
        assert(pte != ((PTE_ADDR >> 1) & PTE_ADDR));
        return 0;
    }

    return (pte & PTE_ADDR) + misalignment;
}

static __always_inline int mphysranges_enum(
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

struct mphysranges_state_t {
    mmphysrange_t *range;
    size_t ranges_count;
    size_t count;
    size_t max_size;
    physaddr_t last_end;
    mmphysrange_t cur_range;
};

static __always_inline int mphysranges_callback(
        mmphysrange_t range, void *context)
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
    if (unlikely(size == 0))
        return 0;

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

bool mphysranges_split(mmphysrange_t *ranges, size_t &ranges_count,
                         size_t count_limit, uint8_t log2_boundary)
{
    if (unlikely(ranges_count == 0))
        return true;

    size_t boundary = size_t(1) << log2_boundary;

    for (size_t i = ranges_count; i > 0 && ranges_count < count_limit; --i) {
        mmphysrange_t *range = ranges + i - 1;

        uintptr_t end = range->physaddr + range->size;

        uintptr_t chk1 = range->physaddr >> log2_boundary;
        uintptr_t chk2 = (end - 1) >> log2_boundary;

        if (chk1 != chk2) {
            // Needs split

            if (unlikely(ranges_count >= count_limit))
                return false;

            uintptr_t new_end = end & -intptr_t(boundary);
            memmove(range + 1, range, (++ranges_count - i) * sizeof(*range));
            range[0].size = new_end - range[0].physaddr;
            range[1].physaddr = range[0].physaddr + range[0].size;
            range[1].size = end - range[1].physaddr;
        }
    }

    return true;
}

void *mmap_register_device(void *context,
                           uint64_t block_size,
                           uint64_t block_count,
                           int prot,
                           mm_dev_mapping_callback_t callback,
                           void *addr)
{
    mm_dev_mapping_scoped_lock lock(mm_dev_mapping_lock);

    auto ins = find(mm_dev_mappings.begin(), mm_dev_mappings.end(), nullptr);

    mmap_device_mapping_t *mapping = new mmap_device_mapping_t{};
    mapping->base_addr = mmap(addr, block_size * block_count,
                              prot, MAP_DEVICE,
                              int(ins - mm_dev_mappings.begin()),
                              0);

    mapping->context = context;
    mapping->len = block_size * block_count;
    mapping->callback = callback;

    mapping->active_read = -1;

    if (ins == mm_dev_mappings.end())
        mm_dev_mappings.push_back(mapping);
    else
        *ins = mapping;

    return likely(mapping) ? mapping->base_addr : 0;
}

static int mm_dev_map_search(void const *v, void const *k, void *s)
{
    (void)s;
    mmap_device_mapping_t const *mapping = *(mmap_device_mapping_t const **)v;

    if (k < mapping->base_addr)
        return -1;

    void const *mapping_end = (char*)mapping->base_addr + mapping->len;
    if (k > mapping_end)
        return 1;

    return 0;
}

size_t mmu_phys_allocator_t::size_from_highest_page(physaddr_t page_index)
{
    return page_index * sizeof(entry_t);
}

void mmu_phys_allocator_t::init(
        void *addr, physaddr_t begin_, uint8_t log2_pagesz_)
{
    entries = (entry_t*)addr;
    begin = begin_;
    log2_pagesz = log2_pagesz_;
}

void mmu_phys_allocator_t::add_free_space(physaddr_t base, size_t size)
{
    scoped_lock lock_(lock);
    physaddr_t free_end = base + size;
    unsigned low = base < 0x100000000;
    size_t pagesz = uint64_t(1) << log2_pagesz;
    entry_t index = index_from_addr(free_end) - 1;
    while (size != 0) {
        entries[index] = next_free[low];
        next_free[low] = index;
        --index;
        size -= pagesz;
        ++free_page_count;
    }
}

physaddr_t mmu_phys_allocator_t::alloc_one(bool low)
{
    scoped_lock lock_(lock);

    size_t item = next_free[low];

    if (unlikely(!item) && !low)
        item = next_free[1];

    if (unlikely(!item))
        return 0;

    entry_t new_next = entries[item];
    next_free[low] = new_next;
    entries[item] = 1;

    lock_.unlock();

    return addr_from_index(item);
}

template<typename F>
bool mmu_phys_allocator_t::alloc_multiple(bool low, size_t size, F callback)
{
    size_t count = size >> log2_pagesz;

    scoped_lock lock_(lock);

    // Fall back to low memory immediately if no free high memory
    if (!next_free[0])
        low = true;

    entry_t first;

    for (;;) {
        first = next_free[low];
        entry_t new_next = first;
        size_t i;
        for (i = 0; i < count && new_next; ++i)
            new_next = entries[new_next];

        // If we found enough pages, commit the change
        if (i == count) {
            next_free[low] = new_next;
            free_page_count -= count;
            break;
        } else if (low) {
            return false;
        } else {
            low = true;
            continue;
        }
    }

    lock_.unlock();

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    for (size_t i = 0; i < count; ++i) {
        entry_t next = entries[first];

        physaddr_t paddr = addr_from_index(first);

        // Call callable with physical address
        if (callback(i << log2_pagesz, paddr)) {
            // Set reference count to 1
            entries[first] = 1;
        } else {
            free_batch.free(paddr);
        }

        // Follow chain to next free
        first = next;
    }

    return true;
}

void mmu_phys_allocator_t::release_one(physaddr_t addr)
{
    scoped_lock lock_(lock);
    release_one_locked(addr);
}

void mmu_phys_allocator_t::addref(physaddr_t addr)
{
    entry_t index = index_from_addr(addr);
    scoped_lock lock_(lock);
    ++entries[index];
}

void mmu_phys_allocator_t::addref_virtual_range(linaddr_t start, size_t len)
{
    unsigned misalignment = start & PAGE_SCALE;
    start -= misalignment;
    len += misalignment;
    len = round_up(len);

    pte_t *ptes[4];
    ptes_from_addr(ptes, start);

    size_t count = len >> log2_pagesz;

    scoped_lock lock_(lock);

    for (size_t i = 0; i < count; ++i) {
        physaddr_t addr = *ptes[3] & PTE_ADDR;

        if (addr && addr != PTE_ADDR) {
            entry_t index = index_from_addr(addr);
            ++entries[index];
        }

        ++ptes[3];
    }
}

uintptr_t mm_alloc_hole(size_t size)
{
    size = (size + 63) & -64;
    return hole_allocator.alloc_linear(size);
}

void mm_free_hole(uintptr_t addr, size_t size)
{
    hole_allocator.release_linear(addr, size);
}

void *mmap_window(size_t size)
{
    size = round_up(size);

    linaddr_t addr = linear_allocator.alloc_linear(size);

    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        mmu_map_page(addr + i, PTE_ADDR,
                     PTE_ACCESSED | PTE_DIRTY | PTE_WRITABLE);
    }

    return (void*)addr;
}

void munmap_window(void *addr, size_t size)
{
    size = round_up(size);
    linear_allocator.release_linear((uintptr_t)addr, size);
}

int alias_window(void *addr, size_t size,
                  mmphysrange_t const *ranges, size_t range_count)
{
    linaddr_t base = linaddr_t(addr);
    unsigned misalignment = base & PAGE_MASK;
    base -= misalignment;
    size += misalignment;
    size = round_up(size);

    mmphysrange_t const *range = ranges;
    mmphysrange_t const *ranges_end = ranges + range_count;

    size_t range_offset = 0;

    pte_t *ptes[4];
    ptes_from_addr(ptes, base);
    int present_mask = ptes_present(ptes);
    size_t pte_index = 0;

    assert((present_mask & 0x7) == 0x7);
    if ((present_mask & 0x7) != 0x7)
        return 0;

    if (ranges) {
        for (size_t offset = 0; offset < size;
             offset += PAGE_SIZE, range_offset += PAGE_SIZE) {
            // If we have reached the end of the range, advance to next range
            if (range_offset >= range->size) {
                if (++range == ranges_end)
                    return (offset + PAGE_SIZE) == size;
                range_offset = 0;
            }

            ptes[3][pte_index++] =
                    ((range->physaddr + range_offset) & PTE_ADDR) |
                    PTE_ACCESSED | PTE_DIRTY | PTE_WRITABLE | PTE_PRESENT;
        }
    } else {
        for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
            ptes[3][pte_index++] = PTE_ADDR | PTE_ACCESSED |
                    PTE_DIRTY | PTE_WRITABLE;
        }
    }

    for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
        cpu_invalidate_page(base + offset);

    return 1;
}

int mlock(const void *addr, size_t len)
{
    linaddr_t staddr = linaddr_t(addr);
    linaddr_t enaddr = staddr + len;

    bool kernel = staddr >= 0x800000000000;

    if (unlikely(!kernel && enaddr >= 0x800000000000))
        return int(errno_t::EINVAL);



    return 0;
}

uintptr_t mm_new_process(process_t *process)
{
    // Allocate a page directory
    pte_t *dir = (pte_t*)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, 0, -1, 0);

    // Copy upper memory mappings into new page directory
    memcpy(dir + 256, master_pagedir + 256, sizeof(*dir) * 256);

    // Get the physical address for the new process page directory
    physaddr_t dir_physaddr = mphysaddr(dir);

    // Initialize recursive mapping
    dir[PT_RECURSE] = dir_physaddr | PTE_PRESENT | PTE_WRITABLE |
            PTE_ACCESSED | PTE_DIRTY;

    // Switch to new page directory
    cpu_set_page_directory(dir_physaddr);

    cpu_flush_tlb();

    mm_init_process(process);

    return dir_physaddr;
}

void mm_destroy_process()
{
    physaddr_t dir = cpu_get_page_directory() & PTE_ADDR;

    assert(dir != root_physaddr);

    unsigned path[4];
    pte_t *ptes[4];

    vector<physaddr_t> pending_frees;
    pending_frees.reserve(4);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    for (linaddr_t addr = 0; addr <= 0x800000000000; ) {
        for (physaddr_t physaddr : pending_frees)
            free_batch.free(physaddr);
        pending_frees.clear();

        if (addr == 0x800000000000)
            break;

        int present_mask = addr_present(addr, path, ptes);

        if ((present_mask & 0xF) == 0xF &&
                !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
            pending_frees.push_back(*ptes[3] & PTE_ADDR);
        if (unlikely(path[3] == 511)) {
            if ((present_mask & 0x7) == 0x7 &&
                    !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
                pending_frees.push_back(*ptes[2] & PTE_ADDR);
            if (path[2] == 511) {
                if ((present_mask & 0x3) == 0x3 &&
                        !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
                    pending_frees.push_back(*ptes[1] & PTE_ADDR);
                if (path[1] == 511) {
                    if ((present_mask & 0x1) == 0x1 &&
                            !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
                        pending_frees.push_back(*ptes[0] & PTE_ADDR);
                }
            }
        }

        // If level 0 is not present, advance 512GB
        if (unlikely(!(present_mask & 1))) {
            addr += 1L << (12 + (9*3));
            continue;
        }

        // If level 1 is not present, advance 1GB
        if (unlikely(!(present_mask & 2))) {
            addr += 1L << (12 + (9*2));
            continue;
        }

        // If level 2 is not present, advance 2MB
        if (unlikely(!(present_mask & 4))) {
            addr += 1L << (12 + (9*1));
            continue;
        }

        addr += 1L << (12 + (9*0));
    }

    cpu_set_page_directory(root_physaddr);

    free_batch.free(dir);
}

void mm_init_process(process_t *process)
{
    contiguous_allocator_t *allocator = new contiguous_allocator_t{};
    allocator->init(0x400000, 0x800000000000 - 0x400000, "process");
    process->set_allocator(allocator);
}

extern char ___text_st[];
extern char ___text_en[];

// Returns the physical address of the original page directory
uintptr_t mm_fork_kernel_text()
{
    uintptr_t st = uintptr_t(___text_st);
    uintptr_t en = uintptr_t(___text_en) - 1;

    // Make aliasing window big enough for two pages
    pte_t *window = (pte_t*)mmap_window(PAGE_SIZE << 1);
    uintptr_t src_linaddr = uintptr_t(window);
    uintptr_t dst_linaddr = uintptr_t(window + 512);

    pte_t constexpr flags = PTE_PRESENT | PTE_WRITABLE;

    // Clone root page directory
    physaddr_t page = mmu_alloc_phys(0);
    mmu_map_page(uintptr_t(window), page, flags);
    cpu_invalidate_page(uintptr_t(window));
    memcpy(window, master_pagedir, PAGE_SIZE);
    window[PT_RECURSE] = page | flags;

    uintptr_t orig_pagedir = cpu_get_page_directory();
    cpu_set_page_directory(page);

    pte_t *st_ptes[4];
    pte_t *en_ptes[4];

    ptes_from_addr(st_ptes, st);
    ptes_from_addr(en_ptes, en);

    for (size_t level = 0; level < 4; ++level) {
        for (pte_t *pte = st_ptes[level]; pte <= en_ptes[level]; ++pte) {
            // Allocate clone
            page = mmu_alloc_phys(0);

            // Map original
            mmu_map_page(src_linaddr, (*pte & PTE_ADDR), flags);

            // Map clone
            mmu_map_page(dst_linaddr, page, flags);

            cpu_invalidate_page(src_linaddr);
            cpu_invalidate_page(dst_linaddr);

            // Copy original to clone
            memcpy(window + 512, window, PAGE_SIZE);

            atomic_barrier();

            // Point PTE to clone
            *pte = page | flags;
        }
    }
    cpu_flush_tlb();

    return orig_pagedir;
}
