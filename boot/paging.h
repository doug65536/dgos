#pragma once

#include "types.h"

// The entries of the 4 levels of page tables are named:
//  PML4E (maps 512GB region)
//  PDPTE (maps 1GB region)
//  PDE   (maps 2MB region)
//  PTE   (maps 4KB region)

// 4KB pages
#define PAGE_SIZE_BIT       12
#ifndef PAGE_SIZE
#define PAGE_SIZE           (1U << PAGE_SIZE_BIT)
#define PAGE_MASK           (PAGE_SIZE - 1U)
#endif

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

// Size of multi-bit fields
#define PTE_PK_BITS         4
#define PTE_ADDR_BITS       28

// Bitmask for multi-bit field values
// Aligned to bit 0
#define PTE_PK_MASK         ((1ULL << PTE_PK_BITS) - 1U)
#define PTE_ADDR_MASK       ((1ULL << PTE_ADDR_BITS) - 1U)

// Values of bits
#define PTE_PRESENT         (1ULL << PTE_PRESENT_BIT)
#define PTE_WRITABLE        (1ULL << PTE_WRITABLE_BIT)
#define PTE_USER            (1ULL << PTE_USER_BIT)
#define PTE_PWT             (1ULL << PTE_PWT_BIT)
#define PTE_PCD             (1ULL << PTE_PCD_BIT)
#define PTE_ACCESSED        (1ULL << PTE_ACCESSED_BIT)
#define PTE_DIRTY           (1ULL << PTE_DIRTY_BIT)
#define PTE_PAGESIZE        (1ULL << PTE_PAGESIZE_BIT)
#define PTE_PTPAT           (1ULL << PTE_PAGESIZE_BIT)
#define PTE_GLOBAL          (1ULL << PTE_GLOBAL_BIT)
#define PTE_PAT             (1ULL << PTE_PAT_BIT)
#define PTE_NX              (1ULL << PTE_NX_BIT)

#define PTE_EX_PHYSICAL_BIT 9
#define PTE_EX_PHYSICAL     (1 << PTE_EX_PHYSICAL_BIT)

// Multi-bit field masks, in place
#define PTE_ADDR            (PTE_ADDR_MASK << PTE_ADDR_BIT)
#define PTE_PK              (PTE_PK_MASK << PTE_PK_BIT)

extern "C"
_pure uint32_t paging_root_addr();

typedef uint64_t pte_t;
typedef uint64_t addr64_t;
typedef uint64_t size64_t;

struct phys_alloc_t {
    uint64_t base;
    uint64_t size;
};

class page_factory_t {
public:
    virtual phys_alloc_t alloc(size64_t size) noexcept = 0;

protected:
    virtual ~page_factory_t() noexcept {}
};

void paging_map_range(
        page_factory_t *allocator,
        uint64_t linear_base,
        uint64_t length,
        uint64_t pte_flags);

void paging_map_physical(
        uint64_t phys_addr,
        uint64_t linear_base,
        uint64_t length,
        uint64_t pte_flags);

void paging_alias_range(addr64_t alias_addr,
                        addr64_t linear_addr,
                        size64_t size,
                        pte_t alias_flags);

void paging_modify_flags(addr64_t addr, size64_t size,
                         pte_t clear, pte_t set);

uint64_t paging_physaddr_of(uint64_t linear_addr);

struct iovec_t {
    uint64_t base;
    uint64_t size;
};

int paging_iovec(iovec_t **ret, uint64_t vaddr,
                 uint64_t size, uint64_t max_chunk);

off_t paging_iovec_read(int fd, off_t file_offset, uint64_t vaddr,
                        uint64_t size, uint64_t max_chunk);
