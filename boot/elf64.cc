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
#include "bootmenu.h"

#define ELF64_DEBUG    0
#if ELF64_DEBUG
#define ELF64_TRACE(...) PRINT("elf64: " __VA_ARGS__)
#else
#define ELF64_TRACE(...) ((void)0)
#endif

extern "C" void enter_kernel(uint64_t entry_point);

// Save the entry point address for later MP processor startup
uint64_t mp_enter_kernel;

static int64_t base_adj;

static void enter_kernel_initial(uint64_t entry_point)
{
    //uintptr_t vbe_info_vector = vbe_select_mode(65535, 800, 1) << 4;
    //vbe_info_vector = vbe_select_mode(1920, 1080, 1) << 4;

    //
    // Relocate MP entry trampoline to 4KB boundary

    void *ap_entry_ptr = malloc_aligned(ap_entry_size, PAGE_SIZE);

    PRINT("SMP trampoline at 0x%zx:%zx",
          uintptr_t(ap_entry_ptr) >> 4,
          uintptr_t(ap_entry_ptr) & 0xF);

    memcpy(ap_entry_ptr, ap_entry, ap_entry_size);

    //
    // Build physical memory table

    uint32_t phys_mem_table_size = 0;
    void *phys_mem_table = get_ram_regions(&phys_mem_table_size);

    paging_map_range(uint64_t(phys_mem_table),
                     uint64_t(phys_mem_table_size),
                     uint64_t(phys_mem_table),
                     PTE_PRESENT | PTE_WRITABLE, 1);

    PRINT("Mapping aliasing window\n");

    // Map a page that the kernel can use to manipulate
    // arbitrary physical addresses by changing its pte
    paging_map_range((0xFFFFFFFF80000000ULL - PAGE_SIZE) + base_adj,
                     PAGE_SIZE, 0, PTE_PRESENT | PTE_WRITABLE, 0);

    PRINT("Mapping first 768KB\n");

    // Map first 768KB (0x0000-0xBFFF)
    paging_map_range(0, 0xC0000, 0,
                     PTE_PRESENT | PTE_WRITABLE |
                     (-cpu_has_global_pages() & PTE_GLOBAL), 2);

    kernel_params_t params{};

    params.size = sizeof(params);

    params.ap_entry = uintptr_t((void(*)())ap_entry_ptr);
    params.phys_mem_table = uint64_t(phys_mem_table);
    params.phys_mem_table_size = phys_mem_table_size;
    //params.vbe_selected_mode = vbe_info_vector;
    params.boot_drv_serial = boot_serial();

    boot_menu_show(params);

    PRINT("           ap_entry: 0x%llx\n", uint64_t(params.ap_entry));
    PRINT("     phys_mem_table: 0x%llx\n", uint64_t(params.phys_mem_table));
    PRINT("phys_mem_table_size: 0x%llx\n", params.phys_mem_table_size);
    PRINT("  vbe_selected_mode: 0x%llx\n",
               uint64_t(params.vbe_selected_mode));
    PRINT("    boot_drv_serial: 0x%llx\n", params.boot_drv_serial);
    PRINT("    serial_debugout: 0x%llx\n",
               uint64_t(params.serial_debugout));
    PRINT("           wait_gdb: 0x%x\n", params.wait_gdb);

    ELF64_TRACE("Entry point: 0x%llx\n", entry_point);

    PRINT("Entering kernel at 0x%llx\n", entry_point);

    run_kernel(entry_point, &params);
}

void enter_kernel(uint64_t entry_point)
{
    if (mp_enter_kernel == 0) {
        mp_enter_kernel = entry_point;
        enter_kernel_initial(entry_point);
        return;
    }

    run_kernel(entry_point, nullptr);
}

//static void apply_reloc(uint64_t new_base,
//                        Elf64_Rela const *rela, size_t relcnt)
//{
//    int64_t dist = new_base - 0xFFFFFFFF80000000;

//    for (size_t r = 0; r < relcnt; ++r) {
//        PRINT("Relocation ofs=%llx, addend=%llx, info=%llx",
//                   rela[r].r_offset,
//                   rela[r].r_addend,
//                   rela[r].r_info);
//    }
//}

bool elf64_run(char const *filename)
{
    cpu_init();

    //if (!cpu_has_long_mode())
    //    HALT("Need 64-bit CPU");

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
    uintptr_t address_window =
            (uintptr_t)malloc_aligned(PAGE_SIZE << 1, PAGE_SIZE);

    PRINT("Loading %s...\n", filename);

    int file = boot_open(filename);

    if (file < 0)
        HALT("Could not open kernel file");

    ssize_t read_size;
    Elf64_Ehdr file_hdr;

    read_size = boot_pread(file, &file_hdr, sizeof(file_hdr), 0);

    if (read_size != sizeof(file_hdr))
        HALT("Could not read ELF header");

    // Check magic number
    if (memcmp(&file_hdr.e_ident[EI_MAG0],
               elf_magic, sizeof(elf_magic)))
        HALT("Could not read magic number");

    // Load program headers
    Elf64_Phdr *program_hdrs;
    read_size = sizeof(*program_hdrs) * file_hdr.e_phnum;
    program_hdrs = (Elf64_Phdr*)malloc(read_size);
    if (!program_hdrs)
        HALT("Insufficient memory for program headers");
    if (read_size != boot_pread(
                file,
                program_hdrs,
                read_size,
                file_hdr.e_phoff))
        HALT("Could not read program headers");

    uint64_t total_bytes = 0;
    for (unsigned i = 0; i < file_hdr.e_phnum; ++i)
        total_bytes += program_hdrs[i].p_memsz;

    // Load relocations
    if (file_hdr.e_shentsize != sizeof(Elf64_Shdr))
        PRINT("Executable has unexpected section header size");

    ssize_t shbytes = file_hdr.e_shentsize * file_hdr.e_shnum;
    Elf64_Shdr *shdrs = (Elf64_Shdr*)malloc(shbytes);

    if (shbytes != boot_pread(file, shdrs, shbytes, file_hdr.e_shoff))
        HALT("Could not read section headers\n");

    bool failed = false;

    uint64_t done_bytes = 0;

    uint64_t new_base = 0xFFFFFFFF80000000;
    base_adj = new_base - 0xFFFFFFFF80000000;

    PRINT("Loading kernel...");

    // For each program header
    for (size_t i = 0; !failed && i < file_hdr.e_phnum; ++i) {
        Elf64_Phdr *blk = program_hdrs + i;

        blk->p_vaddr += base_adj;

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
                copy_kernel(address_window + page_ofs,
                            read_buffer, chunk_size);
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

    for (size_t i = 0; i < file_hdr.e_shnum; ++i) {
        if (shdrs[i].sh_type == SHT_RELA) {
            Elf64_Rela *rela;
            rela = (Elf64_Rela*)malloc(shdrs[i].sh_size);
            size_t relcnt = shdrs[i].sh_size / sizeof(*rela);

            paging_map_range(uint64_t(rela),
                             uint64_t(rela + relcnt) - uint64_t(rela),
                             uint64_t(rela), PTE_PRESENT | PTE_ACCESSED, 1);

            if (ssize_t(shdrs[i].sh_size) != boot_pread(
                        file, rela, shdrs[i].sh_size, shdrs[i].sh_offset))
                HALT("Could not read relocation section");

            reloc_kernel(base_adj, rela, relcnt);
        }
    }

    boot_close(file);

    paging_modify_flags(address_window, PAGE_SIZE << 1,
                        ~(uint64_t)0, 0);

    free(program_hdrs);

    ELF64_TRACE("Entering kernel");

    if (!failed)
        enter_kernel(file_hdr.e_entry + base_adj);

    HALT("Failed to load kernel");

    return false;
}
