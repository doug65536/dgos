#include "paging.h"
#include "screen.h"
#include "malloc.h"
#include "screen.h"
#include "farptr.h"
#include "ctors.h"
#include "assert.h"
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

// Read a 64-bit entry from the specified slot of the specified segment
static _always_inline pte_t read_pte(pte_t *page, size_t slot)
{
    return page[slot];
}

// Write a 64-bit entry to the specified slot of the specified segment
static _always_inline void write_pte(pte_t *page, size_t slot, pte_t pte)
{
    page[slot] = pte;
}

static void clear_page_table(pte_t *page)
{
    memset(page, 0, 512 * sizeof(pte_t));
}

static pte_t *allocate_page_table()
{
    pte_t *page = (pte_t*)malloc_aligned(PAGE_SIZE, PAGE_SIZE);
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
        pte = read_pte(ref, slot);

        pte_t next_segment = (pte & PTE_ADDR);

        if (next_segment == 0) {
            if (!create)
                return nullptr;

            // Allocate a page table on first use
            next_segment = (pte_t)allocate_page_table();

            pte = next_segment | (PTE_PRESENT | PTE_WRITABLE);
            write_pte(ref, slot, pte);
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
    for (uint64_t addr = linear_base; addr < end; addr += PAGE_SIZE) {
        // Calculate pte pointer at start and at 2MB boundaries
        if (!pte || ((addr & -(1<<21)) == addr))
            pte = paging_find_pte(addr, 12, true);

        if (!(*pte++ & PTE_PRESENT))
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

int paging_iovec(iovec_t **ret, uint64_t vaddr,
                 uint64_t size, uint64_t max_chunk)
{
    size_t capacity = 16;
    iovec_t *iovec = (iovec_t*)malloc(sizeof(**ret) * capacity);

    size_t count = 0;
    size_t misalignment = vaddr & PAGE_MASK;
    uint64_t offset = 0;

    for (pte_t *pte = nullptr; offset < size; ++pte) {
        if (!pte || ((vaddr & -(1<<21)) == vaddr))
            pte = paging_find_pte(vaddr, 12, false);

        if (unlikely(!pte))
            PANIC("Failed to find PTE for iovec");

        if (count + 1 > capacity) {
            iovec = (iovec_t*)realloc(iovec, sizeof(*iovec) * (capacity *= 2));

            if (!iovec)
                PANIC("Out of memory growing iovec array");
        }

        uint64_t paddr = (*pte & PTE_ADDR) + misalignment;
        uint64_t chunk = PAGE_SIZE - misalignment;
        misalignment = 0;

        if (offset + chunk > size)
            chunk = size - offset;

        iovec[count].size = chunk;
        iovec[count].base = paddr;
        paddr += chunk;
        vaddr += chunk;

        // If this entry is contiguous with the previous entry,
        // and it would not exceed the specified max chunk size,
        // then merge them
        if (count && iovec[count-1].base + iovec[count-1].size ==
                iovec[count].base &&
                iovec[count-1].size + chunk <= max_chunk) {
            iovec[count-1].size += iovec[count].size;
        } else {
            ++count;
        }

        offset += chunk;
    }

    assert(offset == size);

    // The caller is likely to leak the memory if we tell them there are zero
    if (!count) {
        free(iovec);
        iovec = nullptr;
    }

    *ret = iovec;
    return count;
}

// Incoming pte_flags should be as if 4KB page (bit 7 is PAT bit)
void paging_map_physical(uint64_t phys_addr, uint64_t linear_base,
                         uint64_t length, uint64_t pte_flags)
{
    // Make sure the flags don't set any address bits
    assert((pte_flags & PTE_ADDR) == 0);

    // Automatically infer the optimal page size
    size_t page_size;
    uint8_t log2_pagesize;

    page_size = 1 << 30;
    for (log2_pagesize = 30; log2_pagesize > 12; log2_pagesize -= 9) {
        if ((phys_addr & -page_size) == phys_addr &&
                (linear_base & -page_size) == linear_base &&
                (length & -page_size) == length) {
            // Use huge page

            // Move PAT bit over to PDPT/PD location
            pte_flags |= unsigned(!!(pte_flags & PTE_PAGESIZE)) << PTE_PAT_BIT;

            // Set PSE bit
            pte_flags |= PTE_PAGESIZE;

            break;
        }
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
        if (!pte || ((vaddr & -(1 << (log2_pagesize + 9))) == vaddr))
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
    paging_map_physical(0, 0, 0x10000, PTE_PRESENT | PTE_WRITABLE);
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

    return (p && *p & PTE_PRESENT) ? (*p & PTE_ADDR) + misalignment : -1;
}
