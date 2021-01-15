#pragma once

#include "types.h"
#include "assert.h"

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

#if defined(__x86_64__) || defined(__i386__)

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
#elif defined(__aarch64__)
#define PTE_PRESENT_BIT     0
#define PTE_TABLE_BIT       1
#define PTE_INDX_BIT        2
#define PTE_NS_BIT          5
#define PTE_AP_BIT          6
#define PTE_SH_BIT          8
#define PTE_AF_BIT          10
#define PTE_ADDR_BIT        12
#define PTE_PXN_BIT         53
#define PTE_UXN_BIT         54
#define PTE_AVAIL_BIT       55

#define PTE_INDX_BITS       3
#define PTE_AP_BITS         2
#define PTE_SH_BITS         2
#define PTE_ADDR_BITS       40
#define PTE_AVAIL_BITS      4

#define PTE_INDX_MASK       (~-(UINT64_C(1) << PTE_INDX_BITS))
#define PTE_AP_MASK         (~-(UINT64_C(1) << PTE_AP_BITS))
#define PTE_SH_MASK         (~-(UINT64_C(1) << PTE_SH_BITS))
#define PTE_ADDR_MASK       (~-(UINT64_C(1) << PTE_ADDR_BITS))
#define PTE_AVAIL_MASK      (~-(UINT64_C(1) << PTE_AVAIL_BITS))

#define PTE_PRESENT         (UINT64_C(1) << PTE_PRESENT)
#define PTE_TABLE           (UINT64_C(1) << PTE_TABLE)
#define PTE_INDX            (UINT64_C(1) << PTE_INDX)
#define PTE_NS              (UINT64_C(1) << PTE_NS)
#define PTE_AP              (UINT64_C(1) << PTE_AP)
#define PTE_SH              (UINT64_C(1) << PTE_SH)
#define PTE_AF              (UINT64_C(1) << PTE_AF)
#define PTE_ADDR            (UINT64_C(1) << PTE_ADDR)
#define PTE_PXN             (UINT64_C(1) << PTE_PXN)
#define PTE_UXN             (UINT64_C(1) << PTE_UXN)
#define PTE_AVAIL           (UINT64_C(1) << PTE_AVAIL)

#define PTE_PRESENT         (UINT64_C(1) << PTE_PRESENT)
#define PTE_TABLE           (UINT64_C(1) << PTE_TABLE)
#define PTE_INDX            (PTE_INDX_MASK << PTE_INDX_BIT)
#define PTE_NS              (UINT64_C(1) << PTE_NS)
#define PTE_AP              (UINT64_C(1) << PTE_AP)
#define PTE_SH              (PTE_SH_MASK << PTE_SH_BIT)
#define PTE_AF              (UINT64_C(1) << PTE_AF)
#define PTE_ADDR            (PTE_ADDR_MASK << PTE_ADDR_BIT)
#define PTE_PXN             (UINT64_C(1) << PTE_PXN)
#define PTE_UXN             (UINT64_C(1) << PTE_UXN)
#define PTE_AVAIL           (PTE_AVAIL_MASK << PTE_AVAIL_BIT)

#endif

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
    virtual phys_alloc_t alloc(size64_t size,
                               uint64_t for_addr = 0) noexcept = 0;

protected:
    virtual ~page_factory_t() noexcept = 0;
};

struct pte_builder_t {
    // Entry is present
    uint64_t p:1;

    // Entry is a huge page
    uint64_t h:1;

    // Entry is global
    uint64_t g:1;

    // Entry is executable
    uint64_t x:1;

    // Entry is writable
    uint64_t w:1;

    // Entry is readable
    uint64_t r:1;

    // Entry is userspace
    uint64_t u:1;

    // reserved zeros
    uint64_t z:57;

    uint64_t physaddr;

    constexpr pte_builder_t()
        : p(0)
        , h(0)
        , g(0)
        , x(0)
        , w(0)
        , r(0)
        , u(0)
        , z(0)
        , physaddr(0)
    {
    }

    constexpr pte_builder_t &urwx(bool u, bool r, bool w, bool x) noexcept
    {
        this->u = u;
        this->r = r;
        this->w = w;
        this->x = x;
        return *this;
    }

    constexpr uint64_t with_addr(int level, uint64_t addr)
    {
        assert(addr < (UINT64_C(1) << 52));
        assert((addr & PAGE_MASK) == 0);
        return (to_pte(level) & ~PTE_ADDR) | addr;
    }

    constexpr pte_builder_t &executable(bool value) noexcept
    {
        x = value;
        return *this;
    }

    constexpr bool executable() const noexcept
    {
        return x;
    }

    constexpr pte_builder_t &readable(bool value) noexcept
    {
        r = value;
        return *this;
    }

    constexpr bool readable() const noexcept
    {
        return r;
    }

    constexpr pte_builder_t &writable(bool value) noexcept
    {
        w = value;
        return *this;
    }

    constexpr bool writable() const noexcept
    {
        return w;
    }

    constexpr uint64_t to_pte(int level) noexcept
    {
#if !defined(__aarch64__)
        return (physaddr & PTE_ADDR) |
                (-r & PTE_PRESENT) |
                (-w & PTE_WRITABLE) |
                (-u & PTE_USER) |
                (-!x & PTE_NX);
#else
    return (physaddr & PTE_ADDR) |
        (-(level < 4) & PTE_TABLE);
#endif
    }
};

void paging_map_range(page_factory_t *allocator,
                      uint64_t linear_base, uint64_t length,
                      pte_t pte_flags);

void paging_map_physical(uint64_t phys_addr, uint64_t linear_base,
                         uint64_t length, uint64_t pte_flags);

void paging_alias_range(addr64_t alias_addr,
                        addr64_t linear_addr,
                        size64_t size,
                        pte_builder_t alias_flags);

//void paging_modify_flags(addr64_t addr, size64_t size,
//                         pte_t clear, pte_t set);

uint64_t paging_physaddr_of(uint64_t linear_addr);

bool paging_access_virtual_memory(uint64_t vaddr, void *data,
                                  size_t data_sz, int is_read);

struct iovec_t {
    uint64_t base;
    uint64_t size;
};

size_t paging_iovec(iovec_t **ret, uint64_t vaddr,
                 uint64_t size, uint64_t max_chunk);

off_t paging_iovec_read(int fd, off_t file_offset, uint64_t vaddr,
                        uint64_t size, uint64_t max_chunk);
