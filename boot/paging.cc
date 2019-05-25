#include "paging.h"
#include "screen.h"
#include "malloc.h"
#include "physmem.h"
#include "screen.h"
#include "string.h"
#include "ctors.h"
#include "assert.h"
#include "fs.h"
#include "halt.h"

// Builds 64-bit page tables
//
// Each page is 4KB and contains 512 64-bit entries
// There are 4 levels of page tables
//  Linear
//  Address
//  Bits
//  -----------
//   47:39 Page directory (512GB regions) (PML4e)
//   38:30 Page directory (1GB regions)   (PDPTE)
//   29:21 Page directory (2MB regions)   (PDE)
//   20:12 Page table     (4KB regions)   (PTE)
//   11:0  Offset (bytes)

#define DEBUG_PAGING	0
#if DEBUG_PAGING
#define PAGING_TRACE(...) PRINT("paging: " __VA_ARGS__)
#else
#define PAGING_TRACE(...) ((void)0)
#endif

_section(".smp.data") pte_t *root_page_dir;

static _always_inline pte_t *pte_ptr(pte_t *page, size_t slot)
{
    return page + slot;
}

static void clear_page_table(pte_t *page)
{
    memset(page, 0, 512 * sizeof(pte_t));
}

static pte_t *allocate_page_table()
{
    phys_alloc_t alloc = alloc_phys(PAGE_SIZE);
    assert(alloc.size == PAGE_SIZE && !(alloc.base & PAGE_MASK));
    pte_t *page = (pte_t*)alloc.base;
    // malloc_aligned(PAGE_SIZE, PAGE_SIZE);
    assert(page != nullptr);
    clear_page_table(page);
    return page;
}

// Returns with segment == 0 if it does mapping does not exist
static pte_t *paging_find_pte(addr64_t linear_addr,
                              uint8_t log2_pagesize,
                              bool create)
{
    pte_t *ref = root_page_dir;
    pte_t pte;

    // Process the address bits from high to low
    // in groups of 9 bits

    size_t slot;

    for (uint8_t shift = 39; ; shift -= 9) {
        // Extract 9 bits of the linear address
        slot = (linear_addr >> shift) & 0x1FF;

        // If we are in the last level page table, then done
        if (shift == log2_pagesize)
            break;

        // Read page table entry
        pte = ref[slot];

        pte_t next_segment = (pte & PTE_ADDR);

        if (next_segment == 0) {
            if (!create)
                return nullptr;

            // Allocate a page table on first use
            next_segment = (pte_t)allocate_page_table();

            pte = next_segment | (PTE_PRESENT | PTE_WRITABLE | PTE_USER);
            ref[slot] = pte;
        }

        ref = (pte_t*)next_segment;
    }

    return ref + slot;
}

void paging_alias_range(addr64_t alias_addr,
                        addr64_t linear_addr,
                        size64_t size,
                        pte_t alias_flags)
{
    PAGING_TRACE("aliasing %llu bytes at lin %llx to physaddr %llx\n",
                 size, linear_addr, alias_addr);

    for (uint64_t offset = 0; offset < size; offset += PAGE_SIZE) {
        pte_t *original = paging_find_pte(linear_addr, 12, false);
        pte_t *alias_ref = paging_find_pte(alias_addr, 12, false);

        if (original) {
            *alias_ref = (*original & PTE_ADDR) | alias_flags;
        } else {
            *alias_ref = alias_flags & ~(PTE_PRESENT | PTE_ADDR);
        }
    }
}

void paging_map_range(
        page_factory_t *allocator,
        uint64_t linear_base,
        uint64_t length,
        uint64_t pte_flags)
{
    size_t misalignment = linear_base & PAGE_MASK;
    linear_base -= misalignment;
    length += misalignment;
    length = (length + PAGE_MASK) & -PAGE_SIZE;

    uint64_t const end = linear_base + length;

    pte_t *pte = nullptr;

    // Scan the region to calculate memory allocation size
    uint64_t needed = 0;
    for (uint64_t addr = linear_base; addr < end; addr += PAGE_SIZE, ++pte) {
        // Calculate pte pointer at start and at 2MB boundaries
        if (!pte || ((addr & -(1<<21)) == addr))
            pte = paging_find_pte(addr, 12, true);

        if (!(*pte & PTE_PRESENT))
            needed += PAGE_SIZE;
    }

    pte = nullptr;

    // Rescan the region and assign pages to uncommitted page table entries
    phys_alloc_t allocation{};
    for (uint64_t addr = linear_base; addr < end; addr += PAGE_SIZE, ++pte) {
        // Calculate pte pointer at start and at 2MB boundaries
        if (!pte || ((addr & -(1<<21)) == addr))
            pte = paging_find_pte(addr, 12, true);

        if ((*pte & PTE_PRESENT))
            continue;

        // At start and when allocation runs out of space, allocate more
        if (!allocation.size) {
            allocation = allocator->alloc(needed);
            needed -= allocation.size;
        }

        *pte = allocation.base | pte_flags | PTE_PRESENT;
        allocation.base += PAGE_SIZE;
        allocation.size -= PAGE_SIZE;
    }
}

size_t paging_iovec(iovec_t **ret, uint64_t vaddr,
                 uint64_t size, uint64_t max_chunk)
{
    size_t capacity = 16;
    iovec_t *iovec = (iovec_t*)malloc(sizeof(**ret) * capacity);

    size_t count = 0;
    size_t misalignment = vaddr & PAGE_MASK;
    uint64_t offset = 0;

    for (pte_t *pte = nullptr; offset < size; ++pte) {
        if (unlikely(!pte || ((vaddr & -(1 << 21)) == vaddr)))
            pte = paging_find_pte(vaddr, 12, false);

        if (unlikely(!pte))
            PANIC("Failed to find PTE for iovec");

        if (unlikely(count + 1 > capacity)) {
            iovec = (iovec_t*)realloc(iovec, sizeof(*iovec) * (capacity *= 2));

            if (!iovec)
                PANIC("Out of memory growing iovec array");
        }

        uint64_t paddr = (*pte & PTE_ADDR) + misalignment;
        uint64_t chunk = PAGE_SIZE - misalignment;
        misalignment = 0;

        if (offset + chunk > size)
            chunk = size - offset;

        auto& iovec_curr = iovec[count];
        iovec_curr.size = chunk;
        iovec_curr.base = paddr;
        paddr += chunk;
        vaddr += chunk;

        if (count) {
            auto& iovec_last = iovec[count-1];

            // If this entry is contiguous with the previous entry
            if (iovec_last.base + iovec_last.size == iovec_curr.base) {
                // Compute how much we can add onto the previous entry
                auto max_coalesce = max_chunk - iovec_last.size;

                if (max_coalesce >= iovec_curr.size) {
                    // Coalesce entire incoming chunk with previous one
                    iovec_last.size += iovec_curr.size;
                } else if (max_coalesce > 0) {
                    // Partially extend previous one + another with remainder
                    iovec_last.size += max_coalesce;
                    iovec_curr.base += max_coalesce;
                    iovec_curr.size -= max_coalesce;
                    ++count;
                } else {
                    // Cannot extend previous one at all, it is at max
                    // Just keep the one we have
                    ++count;
                }
            }
        } else {
            ++count;
        }

        offset += chunk;
    }

    assert(offset == size);

    // The caller is likely to leak the memory if we tell them there are zero
    if (unlikely(!count)) {
        free(iovec);
        iovec = nullptr;
    }

    *ret = iovec;
    return count;
}

off_t paging_iovec_read(int fd, off_t file_offset,
                        uint64_t vaddr, uint64_t size,
                        uint64_t max_chunk)
{
    uint64_t offset = 0;

    while (offset < size) {
        iovec_t *iovec = nullptr;
        size_t iovec_count = paging_iovec(
                    &iovec, vaddr + offset, size - offset, max_chunk);

        for (size_t i = 0; i < iovec_count; ++i) {
            ssize_t read = boot_pread(
                        fd, (void*)iovec[i].base, iovec[i].size,
                        file_offset + offset);

            if (read != ssize_t(iovec[i].size))
                PANIC("Disk read error");

            offset += iovec[i].size;
        }
    }

    return offset;
}

static inline constexpr uint64_t low_bits(uint64_t value, uint8_t log2n)
{
    return value & ((1 << log2n) - 1);
}

static inline constexpr uint64_t round_up(uint64_t value, uint8_t log2n)
{
    return (value + ((1 << log2n) - 1)) & -(1 << log2n);
}

static inline constexpr uint64_t round_dn(uint64_t value, uint8_t log2n)
{
    return value & -(1 << log2n);
}

void paging_map_physical_impl(uint64_t phys_addr, uint64_t linear_base,
                              uint64_t length, uint64_t pte_flags);

// Incoming pte_flags should be as if 4KB page (bit 7 is PAT bit)
void paging_map_physical(uint64_t phys_addr, uint64_t linear_base,
                         uint64_t length, uint64_t pte_flags)
{
    if (unlikely(low_bits(phys_addr, 12) != low_bits(linear_base, 12)))
    {
        assert_msg(false,
                   TSTR "Impossible linear->physical mapping,"
                   TSTR " bits 11:0 of the physical and linear address"
                   TSTR " must be equal");
        return;
    }

    // -------------------------========---------------------------  //
    //                                                               //
    //                          4KB Only                             //
    //                                                               //
    //   <- phys_addr                        phys_addr + length ->   //
    //   <- linear_base                                            B //
    //                                                             C //
    //                                                             D //
    //                                                             E //
    // A <------------------------ r ----------------------------> F //
    // |                                                           | //
    // | L                                                         | //
    // | |                                                         | //
    // ↓ ↓                                                         ↓ //
    // +-----------------------------------------------------------+ //
    // |                        4K pg ...                          | //
    // +-----------------------------------------------------------+ //
    // ↑                                                           ↑ //
    // |                                                           | //
    // 4K                                                         4K //
    // \------------------------ alignment ------------------------/ //
    //                                                               //
    //                                                               //
    // --------------------------=======---------------------------  //
    //                                                               //
    //                           4KB/2MB                             //
    //                                                               //
    // r_up(A,21) -↘                                   ↙- r_dn(F,21) //
    //                                                 C             //
    //             ↙                                   D             //
    // A <-- r --> B <------------- s ---------------> E <-- v --> F //
    // |           |                                   |           | //
    // | L         |                                   |           | //
    // | |         |                                   |           | //
    // ↓ ↓         ↓                                   ↓           ↓ //
    // +-----------+-----------------------------------+-----------+ //
    // | 4K pg ... |             2M pg ...             | 4K pg ... | //
    // +-----------+-----------------------------------+-----------+ //
    // ↑           ↑                                   ↑           ↑ //
    // |           |                                   |           | //
    // 4K          2M                                  2M         4K //
    // \------------------------ alignment ------------------------/ //
    //                                                               //
    //                                                               //
    // ------------------------===========-------------------------  //
    //                                                               //
    //                         4KB/2MB/1GB                           //
    //                                                               //
    //                                                               //
    //              r_up(B,30)-↘           ↙-r_dn(E,30)              //
    //                         |           |                         //
    // A <-- r --> B <-- s --> C <-- t --> D <-- u --> E <-- v --> F //
    // |           |           |           |           |           | //
    // | L         |           |           |           |           | //
    // | |         |           |           |           |           | //
    // ↓ ↓         ↓           ↓           ↓           ↓           ↓ //
    // +-----------+-----------+-----------+-----------+-----------+ //
    // | 4K pgs... | 2M pgs... | 1G pgs... | 2M pgs... | 4K pgs... | //
    // +-----------+-----------+-----------+-----------+-----------+ //
    // ↑           ↑           ↑           ↑           ↑           ↑ //
    // |           |           |           |           |           | //
    // 4K          2M          1G          1G          2M         4K //
    // \------------------------ alignment ------------------------/ //
    //                                                               //
    //                                                               //
    // A is phys_addr rounded down to a 4KB boundary                 //
    // F is phys_addr + length rounded up to a 4KB boundary          //
    // B is A rounded up to a 2MB boundary                           //
    // C is B rounded up to a 1GB boundary                           //
    // E is F rounded down to a 1MB boundary                         //
    // D is E rounded down to a 1GB boundary                         //
    //                                                               //
    // r = B - A (4KB pages)                                         //
    // s = C - B (2MB pages)                                         //
    // t = D - C (1GB pages)                                         //
    // u = E - D (2MB pages)                                         //
    // v = F - E (4KB pages)                                         //
    //                                                               //
    // Usually, several regions are empty. When alignment permits,   //
    // only the largest page sizes will be used. It starts as a      //
    // single run of 4KB pages, then a run of 2MB regions is carved  //
    // out of it, eliminating some or all 4KB runs, then a run of    //
    // 1GB pages is carved out of it, eliminating some or all of the //
    // 2MB runs                                                      //

    // It's impossible to use a 1GB mapping if the low 30 bits of the
    // physical address and the linear address are not equal
    bool can_use_1G = low_bits(phys_addr, 30) == low_bits(linear_base, 30);

    // It's impossible to use a 2MB mapping if the low 21 bits of the
    // physical address and the linear address are not equal
    bool can_use_2M = low_bits(phys_addr, 21) == low_bits(linear_base, 21);

    uint64_t phys_end = phys_addr + length;

    uint64_t A, B, C, D, E, F, X, Y;

    // Start with a simple run of 4KB pages
    A = round_dn(phys_addr, 12);
    F = round_up(phys_end, 12);
    B = C = D = E = F;

    // Compute 2MB rounded boundaries for B,C/D/E
    X = round_up(A, 21);
    Y = round_dn(F, 21);

    if (X < Y && can_use_2M)
    {
        B = X;
        C = D = E = Y;
    }

    // Compute 1GB rounded boundaries for C,D
    X = round_up(X, 30);
    Y = round_dn(Y, 30);

    if (X < Y && can_use_1G)
    {
        C = X;
        D = Y;
    }

    int64_t r = B - A;  // 4KB
    int64_t s = C - B;  // 2MB
    int64_t t = D - C;  // 1GB
    int64_t u = E - D;  // 2MB
    int64_t v = F - E;  // 4KB

    int64_t phys_to_virt = linear_base - phys_addr;

    // 4KB page region
    if (r > 0)
        paging_map_physical_impl(A, A + phys_to_virt, r, pte_flags);

    // 2MB page region
    if (s > 0)
        paging_map_physical_impl(B, B + phys_to_virt, s, pte_flags);

    // 1GB page region
    if (t > 0)
        paging_map_physical_impl(C, C + phys_to_virt, t, pte_flags);

    // 2MB page region
    if (u > 0)
        paging_map_physical_impl(D, D + phys_to_virt, u, pte_flags);

    // 4KB page region
    if (v > 0)
        paging_map_physical_impl(E, E + phys_to_virt, v, pte_flags);
}

void paging_map_physical_impl(uint64_t phys_addr, uint64_t linear_base,
                              uint64_t length, uint64_t pte_flags)
{
    // Mask off bit 63:48
    linear_base &= 0xFFFFFFFFFFFF;

    // Make sure the flags don't set any address bits
    assert((pte_flags & PTE_ADDR) == 0);

    // Automatically infer the optimal page size
    uint64_t page_size;
    uint8_t log2_pagesize;

    // Try 1GB, 2MB, 4KB pages, select largest size that would work
    page_size = 1 << 30;
    for (log2_pagesize = 30; log2_pagesize > 12; log2_pagesize -= 9) {
        // If the physical address, virtual address,
        // and length are suitably aligned...
        if ((phys_addr & -page_size) == phys_addr &&
                (linear_base & -page_size) == linear_base &&
                (length & -page_size) == length) {
            // ...then use huge page

            // Move PAT bit over to PDPT/PD location
            pte_flags |= unsigned(!!(pte_flags & PTE_PAGESIZE)) << PTE_PAT_BIT;

            // Set PSE bit
            pte_flags |= PTE_PAGESIZE;

            break;
        }

        // Try the next smaller page size
        page_size >>= 9;
    }

    // Make sure the parameters are feasible
    assert((phys_addr & (page_size - 1)) == (linear_base & (page_size - 1)));

    // Page align and round length up to a multiple of the page size
    size_t misalignment = linear_base & (page_size - 1);
    linear_base -= misalignment;
    phys_addr -= misalignment;
    length += misalignment;
    length = (length + page_size - 1) & -page_size;

    uint64_t end = linear_base + length;

    pte_t *pte = nullptr;

    for (uint64_t vaddr = linear_base; vaddr < end; vaddr += page_size) {
        // Calculate pte pointer at start
        // or when transitioning to another table
        if (!pte || ((vaddr & -(UINT64_C(1) << (log2_pagesize + 9))) == vaddr))
            pte = paging_find_pte(vaddr, log2_pagesize, true);

        *pte++ = phys_addr | pte_flags;
        phys_addr += page_size;
    }
}

uint32_t paging_root_addr()
{
    return uintptr_t(root_page_dir);
}

// Identity map the first 64KB of physical addresses and
// prepare to populate tables
_constructor(500) void paging_init()
{
    // Clear the root page directory
    root_page_dir = (pte_t*)malloc_aligned(PAGE_SIZE, PAGE_SIZE);

    clear_page_table(root_page_dir);

    // Identity map first 64KB
    paging_map_physical(0, 0, 0x10000, PTE_PRESENT |
                        PTE_WRITABLE | PTE_EX_PHYSICAL);
}

void paging_modify_flags(addr64_t addr, size64_t size,
                         pte_t clear, pte_t set)
{
    for (addr64_t offset = 0; offset < size; offset += PAGE_SIZE) {
        pte_t *original = paging_find_pte(addr + offset, 12, false);
        if (original) {
            pte_t pte = *original;
            pte &= ~clear;
            pte |= set;
            *original = pte;
        }
    }
}

uint64_t paging_physaddr_of(uint64_t linear_addr)
{
    unsigned misalignment = linear_addr & PAGE_MASK;

    pte_t *p = paging_find_pte(linear_addr - misalignment, 12, false);

    return (p && (*p & PTE_PRESENT)) ? (*p & PTE_ADDR) + misalignment : -1;
}
