#pragma once

#include "types.h"
#include "cpu/isr.h"
#include "cxxexception.h"
#include "export.h"

__BEGIN_DECLS

#define DEBUG_PHYS_ALLOC        0

typedef uintptr_t physaddr_t;
typedef uintptr_t linaddr_t;

HIDDEN extern physaddr_t root_physaddr;

//HIDDEN extern void mmu_init(int ap);
uintptr_t mm_create_process(void);
void mm_destroy_process(void);

extern "C" isr_context_t *mmu_page_fault_handler(int intr, isr_context_t *ctx);

#define PAGE_SIZE_BIT 12
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
#define PTE_PTEPAT_BIT      7   // only PTE
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
#define PTE_EX_FILEMAP_BIT  (PTE_AVAIL2_BIT+1)
#define PTE_EX_DEMAND_BIT   (PTE_AVAIL2_BIT+2)

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
#define PTE_EX_FILEMAP      (1UL << PTE_EX_FILEMAP_BIT)
#define PTE_EX_DEMAND       (1UL << PTE_EX_DEMAND_BIT)

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

#define PT_RECURSE      UINT64_C(256)
#define PT_BEGIN        (PT_RECURSE << 39)

// Indices of start of page tables for each level
#define PT3_INDEX       (PT_ENTRY(PT_RECURSE,0,0,0))
#define PT2_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,0,0))
#define PT1_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,PT_RECURSE,0))
#define PT0_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,PT_RECURSE,PT_RECURSE))

// Canonicalize the given address
#define CANONICALIZE(n) uintptr_t(intptr_t(n << 16) >> 16)

#define PT3_ADDR        (CANONICALIZE(PT3_INDEX*sizeof(pte_t)))
#define PT2_ADDR        (CANONICALIZE(PT2_INDEX*sizeof(pte_t)))
#define PT1_ADDR        (CANONICALIZE(PT1_INDEX*sizeof(pte_t)))
#define PT0_ADDR        (CANONICALIZE(PT0_INDEX*sizeof(pte_t)))

#define PT3_PTR         ((pte_t*)PT3_ADDR)
#define PT2_PTR         ((pte_t*)PT2_ADDR)
#define PT1_PTR         ((pte_t*)PT1_ADDR)
#define PT0_PTR         ((pte_t*)PT0_ADDR)
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
#define PTE_PTEPAT_BIT      7   // only PTE
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

#define PT_RECURSE      UINT64_C(256)
#define PT_BEGIN        (PT_RECURSE << 39)

// Indices of start of page tables for each level
#define PT3_INDEX       (PT_ENTRY(PT_RECURSE,0,0,0))
#define PT2_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,0,0))
#define PT1_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,PT_RECURSE,0))
#define PT0_INDEX       (PT_ENTRY(PT_RECURSE,PT_RECURSE,PT_RECURSE,PT_RECURSE))

// Canonicalize the given address
#define CANONICALIZE(n) uintptr_t(intptr_t(n << 16) >> 16)

#define PT3_ADDR        (CANONICALIZE(PT3_INDEX*sizeof(pte_t)))
#define PT2_ADDR        (CANONICALIZE(PT2_INDEX*sizeof(pte_t)))
#define PT1_ADDR        (CANONICALIZE(PT1_INDEX*sizeof(pte_t)))
#define PT0_ADDR        (CANONICALIZE(PT0_INDEX*sizeof(pte_t)))

#define PT3_PTR         ((pte_t*)PT3_ADDR)
#define PT2_PTR         ((pte_t*)PT2_ADDR)
#define PT1_PTR         ((pte_t*)PT1_ADDR)
#define PT0_PTR         ((pte_t*)PT0_ADDR)

static inline constexpr size_t round_up(size_t n)
{
    return (n + PAGE_MASK) & -PAGE_SIZE;
}

static inline constexpr size_t round_down(size_t n)
{
    return n & -PAGE_SIZE;
}

static _always_inline void ptes_from_addr(pte_t **pte, linaddr_t addr)
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

static _always_inline bool pte_is_sysmem(pte_t pte)
{
    return (pte & (PTE_PRESENT | PTE_EX_PHYSICAL |
                   PTE_EX_DEVICE | PTE_EX_DEMAND)) ==
            PTE_PRESENT;
}

// True if demand paged
_const
static _always_inline bool pte_is_demand(pte_t pte)
{
    return pte & PTE_EX_DEMAND;
}

// True if device cache page
_const
static _always_inline bool pte_is_device(pte_t pte)
{
    return pte & PTE_EX_DEVICE;
}

// True if page of address space is dedicated to faulting on every access
_const
static _always_inline bool pte_is_guard(pte_t pte)
{
    // Guard pages are represented by a not present page
    // that has a physaddr field of all 1's except 0 in MSB.
    // That's 0x0007FFFFFFFFF000
    return (pte & (PTE_ADDR | PTE_PRESENT | PTE_EX_DEMAND)) ==
            ((PTE_ADDR >> 1) & PTE_ADDR);
}

__END_DECLS
