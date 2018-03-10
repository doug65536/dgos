#include "elf64.h"
#include "elf64decl.h"
#include "string.h"
#include "fs.h"
#include "screen.h"
#include "malloc.h"
#include "paging.h"
#include "cpu.h"
#include "bootsect.h"
#include "physmem.h"
#include "mpentry.h"
#include "farptr.h"
#include "vesa.h"
#include "progressbar.h"
#include "bootloader.h"
#include "driveinfo.h"

#define ELF64_DEBUG    0
#if ELF64_DEBUG
#define ELF64_TRACE(...) print_line("elf64: " __VA_ARGS__)
#else
#define ELF64_TRACE(...) ((void)0)
#endif

extern "C" void enter_kernel(uint64_t entry_point);

// Save the entry point address for later MP processor startup
uint64_t mp_enter_kernel;

static void enter_kernel_initial(uint64_t entry_point)
{
    uintptr_t vbe_info_vector = vbe_select_mode(65535, 800, 1) << 4;
    //vbe_info_vector = vbe_select_mode(1920, 1080, 1) << 4;

    //
    // Relocate MP entry trampoline to 4KB boundary

    uint16_t ap_entry_seg = far_malloc_aligned(ap_entry_size);
    far_ptr_t ap_entry_ptr = far_ptr2(ap_entry_seg, 0);

    print_line("SMP trampoline at 0x%x:%x",
               ap_entry_ptr.segment, ap_entry_ptr.offset);

    far_copy(ap_entry_ptr, far_ptr2(0, uint16_t(uintptr_t(ap_entry))),
             ap_entry_size);

    // Write address of AP entrypoint to ap_entry_vector
    uintptr_t ap_entry_vector = (uintptr_t)ap_entry_seg << 4;

    //
    // Build physical memory table

    uint32_t phys_mem_table_size = 0;
    uint32_t phys_mem_table =
            ((uint32_t)get_ram_regions(&phys_mem_table_size) << 4);

    paging_map_range(phys_mem_table, phys_mem_table_size,
                     phys_mem_table, PTE_PRESENT | PTE_WRITABLE, 1);

    print_line("Mapping aliasing window\n");

    // Map a page that the kernel can use to manipulate
    // arbitrary physical addresses by changing its pte
    paging_map_range(0xFFFFFFFF80000000ULL - PAGE_SIZE, PAGE_SIZE, 0,
                     PTE_PRESENT | PTE_WRITABLE, 0);

    print_line("Mapping first 768KB\n");

    // Map first 768KB (0x0000-0xBFFF)
    paging_map_range(0, 0xC0000, 0,
                     PTE_PRESENT | PTE_WRITABLE |
                     (-cpu_has_global_pages() & PTE_GLOBAL), 2);

    kernel_params_t params;

    params.size = sizeof(params);

    params.mp_entry = ap_entry_vector;
    params.phys_mem_table = phys_mem_table;
    params.phys_mem_table_size = phys_mem_table_size;
    params.vbe_selected_mode = vbe_info_vector;
    params.boot_device_info = boot_device_info_vector;

    ELF64_TRACE("Entry point: 0x%llx\n", entry_point);

    print_line("Entering kernel at 0x%llx\n", entry_point);

    copy_or_enter(entry_point, 0, uint32_t(uintptr_t(&params)));
}

void enter_kernel(uint64_t entry_point)
{
    if (mp_enter_kernel == 0) {
        mp_enter_kernel = entry_point;
        enter_kernel_initial(entry_point);
        return;
    }

    copy_or_enter(entry_point, 0, 0);
}

bool elf64_run(char const *filename)
{
    cpu_init();

    if (!cpu_has_long_mode())
        halt("Need 64-bit CPU");

    uint64_t pge_page_flags = 0;
    if (cpu_has_global_pages())
        pge_page_flags |= PTE_GLOBAL;

    uint64_t nx_page_flags = 0;
    if (cpu_has_no_execute())
        nx_page_flags = PTE_NX;

    // Map the screen for debugging convenience
    paging_map_range(0xA0000, 0x20000, 0xA0000,
                     PTE_PRESENT | PTE_WRITABLE |
                     (-cpu_has_global_pages() & PTE_GLOBAL) |
                     PTE_PCD | PTE_PWT, 0);

    // Allocate a page of memory to be used to alias high memory
    // Map two pages to simplify copies that are not page aligned
    uint32_t address_window =
            (uint32_t)far_malloc_aligned(PAGE_SIZE << 1) << 4;

    print_line("Loading %s...\n", filename);

    int file = boot_open(filename);

    if (file < 0)
        halt("Could not open kernel file");

    ssize_t read_size;
    Elf64_Ehdr file_hdr;

    read_size = boot_pread(file, &file_hdr, sizeof(file_hdr), 0);

    if (read_size != sizeof(file_hdr))
        halt("Could not read ELF header");

    // Check magic number
    if (memcmp(&file_hdr.e_ident[EI_MAG0],
               elf_magic, sizeof(elf_magic)))
        halt("Could not read magic number");

    // Load program headers
    Elf64_Phdr *program_hdrs;
    read_size = sizeof(*program_hdrs) * file_hdr.e_phnum;
    program_hdrs = (Elf64_Phdr*)malloc(read_size);
    if (!program_hdrs)
        halt("Insufficient memory for program headers");
    if (read_size != boot_pread(
                file,
                program_hdrs,
                read_size,
                file_hdr.e_phoff))
        halt("Could not read program headers");

    uint64_t total_bytes = 0;
    for (unsigned i = 0; i < file_hdr.e_phnum; ++i)
        total_bytes += program_hdrs[i].p_memsz;

    bool failed = false;

    uint64_t done_bytes = 0;

    print_line("Loading kernel...");

    // For each section
    for (size_t i = 0; !failed && i < file_hdr.e_phnum; ++i) {
        Elf64_Phdr *blk = program_hdrs + i;

        uint64_t page_flags = (-cpu_has_global_pages() & PTE_GLOBAL);

        // If it is not readable, writable or executable, ignore
        if ((blk->p_flags & (PF_R | PF_W | PF_X)) == 0)
            continue;

        ELF64_TRACE("vaddr=0x%llx, filesz=0x%llx, memsz=0x%llx, paddr=0x%llx",
                   blk->p_vaddr,
                   blk->p_filesz,
                   blk->p_memsz,
                   blk->p_paddr);

        if (blk->p_memsz == 0)
            continue;

        // Pages present
        page_flags |= PTE_PRESENT;

        // Global if possible
        page_flags |= pge_page_flags;

        // If not executable, mark as no execute
        if ((blk->p_flags & PF_X) == 0)
            page_flags |= nx_page_flags;

        // Writable
        if ((blk->p_flags & PF_W) != 0)
            page_flags |= PTE_WRITABLE;

        char read_buffer[PAGE_SIZE];

        uint64_t paddr = blk->p_paddr;

        ssize_t chunk_size;
        uint64_t vaddr = blk->p_vaddr;
        uint64_t remain = blk->p_memsz;
        Elf64_Off file_remain = blk->p_filesz;
        Elf64_Off bss_ofs = blk->p_offset + blk->p_filesz;
        Elf64_Off bss_remain = blk->p_memsz - blk->p_filesz;
        for (Elf64_Off ofs = blk->p_offset,
             end = blk->p_offset + remain; ofs < end;
             ofs += chunk_size, remain -= chunk_size) {
            chunk_size = PAGE_SIZE;

            // Handle partial page,
            // limit chunk to realign to page on next iteration
            uint32_t page_ofs = vaddr & PAGE_MASK;
            chunk_size -= page_ofs;

            // Handle partial read/zero
            if (ofs < bss_ofs) {
                if ((Elf64_Off)chunk_size > file_remain)
                    chunk_size = file_remain;
                file_remain -= chunk_size;
            } else if (ofs >= bss_ofs) {
                if (chunk_size > bss_remain)
                    chunk_size = bss_remain;
                bss_remain -= chunk_size;
            }

            done_bytes += chunk_size;

            if (ofs < bss_ofs) {
                ELF64_TRACE("Reading 0x%lx byte chunk at offset 0x%llx",
                           chunk_size, ofs);
                if (chunk_size != boot_pread(
                            file, read_buffer, chunk_size, ofs))
                    break;
            } else if (ofs == bss_ofs) {
                ELF64_TRACE("Clearing buffer for bss");
                memset(read_buffer, 0, sizeof(read_buffer));
            }

            ELF64_TRACE("Copying 0x%lx bytes to 0x%llx (phys=0x%llx)",
                        chunk_size, vaddr, paddr);

            uint64_t page_vaddr = vaddr - page_ofs;
            uint64_t page_paddr = paddr - page_ofs;

            // Map pages
            paging_map_range(page_vaddr, PAGE_SIZE, page_paddr,
                        page_flags, 1);

            if (paddr + chunk_size <= 0x100000000U) {
                memcpy((char*)paddr, read_buffer, chunk_size);
            } else {
                paging_alias_range(address_window, page_vaddr, PAGE_SIZE << 1,
                                   PTE_PRESENT | PTE_WRITABLE);

                // Copy to alias region
                // Add misalignment offset
                copy_or_enter(address_window + page_ofs,
                              (uint32_t)read_buffer, chunk_size);
            }

            paddr += chunk_size;
            vaddr += chunk_size;

            progress_bar_draw(20, 10, 70, int((100 * done_bytes) /
                                              total_bytes));
        }

        // Clear modified bits if uninitialized data
        if (blk->p_memsz > blk->p_filesz) {
            paging_modify_flags(blk->p_vaddr + blk->p_filesz,
                                blk->p_memsz - blk->p_filesz,
                                PTE_DIRTY | PTE_ACCESSED, 0);
        }
    }
    boot_close(file);

    paging_modify_flags(address_window, PAGE_SIZE << 1,
                        ~(uint64_t)0, 0);

    free(program_hdrs);

    ELF64_TRACE("Entering kernel");

    if (!failed)
        enter_kernel(file_hdr.e_entry);

    halt("Failed to load kernel");

    return false;
}
