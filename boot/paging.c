#include "code16gcc.h"
#include "paging.h"
#include "screen.h"

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

// Start allocating page tables at address 0x10000
// Preincrement it by 0x1000 to allocate a new page table
static uint16_t page_allocator = 0x1000;

// Read a 64-bit entry from the specified slot of the specified segment
static uint64_t read_pte(uint16_t segment, uint16_t slot)
{
    uint64_t pte;
    __asm__ __volatile__ (
        "movw %1,%%fs\n\t"
        "movl %%fs:(,%2,8),%%eax\n\t"
        "movl %%fs:4(,%2,8),%%edx\n\t"
        : "=A" (pte)
        : "r" (segment), "r" ((uint32_t)slot)
        : "memory"
    );
    return pte;
}

// Write a 64-bit entry to the specified slot of the specified segment
static void write_pte(uint16_t segment, uint16_t slot, uint64_t pte)
{
    __asm__ __volatile__ (
        "mov %1,%%fs\n\t"
        "mov %%eax,%%fs:(,%2,8)\n\t"
        "mov %%edx,%%fs:4(,%2,8)\n\t"
        :
        : "A" (pte), "r" (segment), "r" ((uint32_t)slot)
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
    uint16_t segment = (page_allocator += 0x100);
    clear_page_table(segment);
    return segment;
}

static void paging_map_page(
        uint64_t linear_addr,
        uint64_t phys_addr,
        uint64_t pte_flags)
{
    // Root page direcory is at segment 0x1000
    uint16_t segment = 0x1000;
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
            print_line("Creating page directory for %lx",
                       (uint64_t)(linear_addr >> shift) << shift);

            // Allocate a page table on first use
            next_segment = allocate_page_table();

            pte = (uint64_t)next_segment << 4;
            pte |= PTE_PRESENT | PTE_WRITABLE;
            write_pte(segment, slot, pte);
        }

        segment = next_segment;
    }

    print_line("mapping %lx to physaddr %lx pageseg=%x slot=%x",
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
    return 0x10000;
}

// Identity map the first 64KB of physical addresses and
// prepare to populate tables
void paging_init(void)
{
    // Clear the root page directory
    uint16_t segment = 0x1000;

    clear_page_table(segment);

    // Identity map first 64KB
    paging_map_range(0, 0x10000, 0, PTE_PRESENT | PTE_WRITABLE);
}

//void paging_copy_far(far_ptr_realmode_t const *dest,
//                     far_ptr_realmode_t const *src,
//                     uint16_t size)
//{
//    __asm__ __volatile__ (
//        "pushw %%ds\n\t"
//        "pushw %%si\n\t"
//        "pushw %%es\n\t"
//        "pushw %%di\n\t"
//
//        "les (%2),%%di\n\t"
//        "lds (%3),%%si\n\t"
//        "cld\n\t"
//        "rep movsb\n\t"
//
//        "popw %%di\n\t"
//        "popw %%es\n\t"
//        "popw %%si\n\t"
//        "popw %%ds\n\t"
//        : "=c" (size)
//        : "0" (size), "d" (dest), "a" (src)
//        : "memory"
//    );
//}
//
//far_ptr_realmode_t paging_far_realmode_ptr2(uint16_t seg, uint16_t ofs)
//{
//    far_ptr_realmode_t ptr;
//    ptr.offset = ofs;
//    ptr.seg = seg;
//    return ptr;
//}
//
//far_ptr_realmode_t paging_far_realmode_ptr(uint32_t addr)
//{
//    return paging_far_realmode_ptr2(addr >> 4, addr & 0x0000F);
//}
