#include "code16gcc.h"
#include "elf64.h"
#include "string.h"
#include "fat32.h"
#include "screen.h"
#include "malloc.h"
#include "paging.h"
#include "cpu.h"
#include "bootsect.h"
#include "elf64decl.h"

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
    copy_or_enter(entry_point, 0, 0);
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
                     PTE_PRESENT | PTE_WRITABLE);

    // For each section
    for (size_t i = 1; !failed && i < file_hdr.e_shnum; ++i) {
        Elf64_Shdr *sec = section_hdrs + i;

        print_line("section %d:"
                   " name=%s"
                   " addr=%lx"
                   " size=%lx"
                   " off=%lx"
                   " flags=%lx"
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
            paging_map_range(addr, PAGE_SIZE, page_alloc, page_flags);

            // Alias a range for copying at the top of real mode memory
            // Which maps to the physical memory of the segment
            // because segment might be read only
            paging_map_range(0x80000, PAGE_SIZE, page_alloc,
                             PTE_PRESENT | PTE_WRITABLE);

            page_alloc += PAGE_SIZE;
            addr += PAGE_SIZE;

            // Copy to alias region
            copy_or_enter(0x80000, (uint32_t)read_buffer, read_size);
        }

        // If we don't read whole section, something went wrong
        if (remain)
            failed = 1;
    }
    boot_close(file);

    free(section_names);
    free(section_hdrs);

    print_line("Entering kernel");

    paging_map_range(0x80000, PAGE_SIZE, 0x100000,
                     PTE_PRESENT | PTE_WRITABLE);

    if (!failed)
        enter_kernel(file_hdr.e_entry);

    return 1;
}
