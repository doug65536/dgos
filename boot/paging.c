#include "code16gcc.h"
#include "paging.h"
#include "screen.h"
#include "malloc.h"

// Builds 64-bit page tables using far pointers in real mode
//
// Each page is 4KB and contains 512 64-bit entries
// There are 4 levels of page tables
//  Linear
//  Address
//  Bits
//  -----------
//   47:39 Page directory (512GB regions)
//   38:30 Page directory (1GB regions)
//   29:21 Page directory (2MB regions)
//   20:12 Page table (4KB regions)
//   11:0  Offset (bytes)

// Page tables are accessed through far pointers and manipulated
// with helper functions that load segment registers
// The fs segment is used and is not preserved

static uint16_t root_page_dir;

// Read a 64-bit entry from the specified slot of the specified segment
static uint64_t read_pte(uint16_t segment, uint16_t slot)
{
    uint64_t pte;
    __asm__ __volatile__ (
        "shl $3,%w[slot]\n\t"
        "movw %[seg],%%fs\n\t"
        "movl %%fs:(%w[slot]),%%eax\n\t"
        "movl %%fs:4(%w[slot]),%%edx\n\t"
        : "=A" (pte),
          [slot] "+bSD" (slot)
        : [seg] "r" (segment)
        : "memory"
    );
    return pte;
}

// Write a 64-bit entry to the specified slot of the specified segment
static void write_pte(uint16_t segment, uint16_t slot, uint64_t pte)
{
    __asm__ __volatile__ (
        "shl $3,%w[slot]\n\t"
        "mov %[seg],%%fs\n\t"
        "mov %%eax,%%fs:(%w[slot])\n\t"
        "mov %%edx,%%fs:4(%w[slot])\n\t"
        : [slot] "+bSD" (slot)
        : "A" (pte), [seg] "r" (segment)
        : "memory"
    );
}

static void clear_page_table(uint16_t segment)
{
    for (uint16_t i = 0; i < 512; ++i)
        write_pte(segment, i, 0);
}

static uint16_t allocate_page_table()
{
    uint16_t segment = far_malloc_aligned(PAGE_SIZE);
    clear_page_table(segment);
    return segment;
}

static void paging_map_page(
        uint64_t linear_addr,
        uint64_t phys_addr,
        uint64_t pte_flags)
{
    uint16_t segment = root_page_dir;
    uint16_t slot;
    uint64_t pte;

    // Process the address bits from high to low
    // in groups of 9 bits

    for (uint8_t shift = 39; ; shift -= 9) {
        // Extract 9 bits of the linear address
        slot = (uint16_t)(linear_addr >> shift) & 0x1FF;

        // Read page table entry
        pte = read_pte(segment, slot);

        // If we are in the last level page table, then done
        if (shift == 12)
            break;

        uint16_t next_segment = (uint16_t)(pte >> 4) & 0xFF00;

        if (next_segment == 0) {
            print_line("Creating page directory for %llx",
                       (uint64_t)(linear_addr >> shift) << shift);

            // Allocate a page table on first use
            next_segment = allocate_page_table();

            pte = (uint64_t)next_segment << 4;
            pte |= PTE_PRESENT | PTE_WRITABLE;
            write_pte(segment, slot, pte);
        }

        segment = next_segment;
    }

    print_line("mapping %llx to physaddr %llx pageseg=%x slot=%x",
               linear_addr, phys_addr,
               segment, slot);

    write_pte(segment, slot, phys_addr | pte_flags);
}

void paging_map_range(
        uint64_t linear_base,
        uint64_t length,
        uint64_t phys_addr,
        uint64_t pte_flags)
{
    for (uint64_t offset = 0; offset < length; offset += 0x1000) {
        paging_map_page(linear_base + offset,
                        phys_addr + offset,
                        pte_flags);
    }
}

uint32_t paging_root_addr(void)
{
    return root_page_dir << 4;
}

// Identity map the first 64KB of physical addresses and
// prepare to populate tables
void paging_init(void)
{
    // Clear the root page directory
    root_page_dir = far_malloc_aligned(PAGE_SIZE);

    clear_page_table(root_page_dir);

    // Identity map first 64KB
    paging_map_range(0, 0x10000, 0, PTE_PRESENT | PTE_WRITABLE);
}
