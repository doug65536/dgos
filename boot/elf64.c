#include "code16gcc.h"
#include "elf64.h"
#include "elf64decl.h"
#include "string.h"
#include "fat32.h"
#include "screen.h"
#include "malloc.h"
#include "paging.h"
#include "cpu.h"
#include "bootsect.h"
#include "physmem.h"

//static unsigned long elf64_hash(unsigned char const *name)
//{
//    unsigned long h = 0, g;
//    while (*name) {
//        h = (h << 4) + *name++;
//        g = h & 0xf0000000;
//        h ^= g >> 24;
//        h &= 0x0fffffff;
//    }
//    return h;
//}

void enter_kernel(uint64_t entry_point);

void enter_kernel(uint64_t entry_point)
{
    uint32_t phys_mem_table_size = 0;
    uint32_t phys_mem_table =
            ((uint32_t)get_ram_regions(&phys_mem_table_size) << 4);

    paging_map_range(phys_mem_table, phys_mem_table_size,
                     phys_mem_table, PTE_PRESENT | PTE_WRITABLE, 1);

    // Pack the size into the high 12 bits
    phys_mem_table |= phys_mem_table_size << 20;

    copy_or_enter(entry_point, 0, phys_mem_table);
}

uint16_t elf64_run(char const *filename)
{
    if (!cpu_has_long_mode())
        halt("Need 64-bit CPU");

    uint64_t nx_page_flags = 0;
    if (cpu_has_no_execute())
        nx_page_flags = PTE_NX;

    int file = boot_open(filename);
    size_t read_size;
    Elf64_Ehdr file_hdr;

    read_size = boot_pread(file,
                           &file_hdr,
                           sizeof(file_hdr),
                           0);

    if (read_size != sizeof(file_hdr))
        return 0;

    // Check magic number
    if (memcmp(&file_hdr.e_ident[EI_MAG0],
               elf_magic, sizeof(elf_magic)))
        return 0;

    // Load section headers
    Elf64_Shdr *section_hdrs;
    read_size = sizeof(*section_hdrs) * file_hdr.e_shnum;
    section_hdrs = malloc(read_size);
    if (!section_hdrs)
        return 0;
    if (read_size != boot_pread(
                file,
                section_hdrs,
                read_size,
                file_hdr.e_shoff))
        return 0;

    // Load section names
    char *section_names;
    read_size = section_hdrs[file_hdr.e_shstrndx].sh_size;
    section_names = malloc(read_size);
    if (!section_names)
        return 0;
    if (read_size != boot_pread(
                file,
                section_names,
                read_size,
                section_hdrs[file_hdr.e_shstrndx].sh_offset))
        return 0;

    // Allocate physical pages above 1MB line
    uint64_t page_alloc = 0x100000;

    uint16_t failed = 0;

    // Map the screen for debugging convenience
    paging_map_range(0xA0000, 0x20000, 0xA0000,
                     PTE_PRESENT | PTE_WRITABLE |
                     PTE_PCD | PTE_PWT, 0);

    // Allocate a page of memory to be used to alias high memory
    // Map two pages to simplify copies that are not page aligned
    uint32_t address_window =
            (uint32_t)far_malloc_aligned(PAGE_SIZE << 1) << 4;

    // For each section
    for (size_t i = 1; !failed && i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr *sec = section_hdrs + i;

        print_line("section %d:"
                   " name=%s"
                   " addr=%llx"
                   " size=%llx"
                   " off=%llx"
                   " flags=%llx"
                   " type=%x",
                   i,
                   section_names + sec->sh_name,
                   sec->sh_addr,
                   sec->sh_size,
                   sec->sh_offset,
                   sec->sh_flags,
                   sec->sh_type);

        uint64_t page_flags = 0;

        // If it is not readable, writable or executable, ignore
        if ((sec->sh_flags & (PF_R | PF_W | PF_X)) == 0)
            continue;

        // If address is 0, definitely no load
        if (sec->sh_addr == 0)
            continue;

        // Pages present
        page_flags |= PTE_PRESENT;

        // If not executable, mark as no execute
        if ((sec->sh_flags & PF_X) == 0)
            page_flags |= nx_page_flags;

        // Writable
        if ((sec->sh_flags & PF_W) != 0)
            page_flags |= PTE_WRITABLE;

        char read_buffer[PAGE_SIZE];

        // Use zeroed buffer for zero initialized sections
        if (sec->sh_type == SHT_NOBITS)
            memset(read_buffer, 0, PAGE_SIZE);

        size_t read_size;
        uint64_t addr = sec->sh_addr;
        uint64_t remain = sec->sh_size;
        for (off_t ofs = sec->sh_offset,
             end = sec->sh_offset + sec->sh_size; ofs < end;
             ofs += read_size, remain -= read_size) {
            read_size = PAGE_SIZE;
            if (read_size > remain)
                read_size = remain;

            // Read from disk if program section
            if (sec->sh_type == SHT_PROGBITS) {
                if (read_size != boot_pread(
                            file, read_buffer,
                            read_size, ofs))
                    break;
            }

            // Map pages
            // If it is not page aligned, map two pages
            page_alloc += paging_map_range(
                        addr, PAGE_SIZE + (addr & (PAGE_SIZE-1)),
                        page_alloc,
                        page_flags, 1) << PAGE_SIZE_BIT;

            paging_alias_range(address_window, addr, PAGE_SIZE,
                               PTE_PRESENT | PTE_WRITABLE);

            addr += PAGE_SIZE;

            // Copy to alias region
            // Add misalignment offset
            copy_or_enter(address_window + (addr & (PAGE_SIZE-1)),
                          (uint32_t)read_buffer, read_size);
        }

        // If we don't read whole section, something went wrong
        if (remain)
            failed = 1;
    }
    boot_close(file);

    free(section_names);
    free(section_hdrs);

    print_line("Entering kernel");

    if (!failed)
        enter_kernel(file_hdr.e_entry);

    return 1;
}
