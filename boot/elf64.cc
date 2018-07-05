#include "elf64.h"
#include "elf64_abstract.h"
#include "halt.h"
#include "string.h"
#include "fs.h"
#include "screen.h"
#include "malloc.h"
#include "paging.h"
#include "cpu.h"
#include "diskio.h"
#include "physmem.h"
#include "mpentry.h"
#include "farptr.h"
#include "vesa.h"
#include "progressbar.h"
#include "bootloader.h"
#include "bootmenu.h"
#include "physmap.h"
#include "qemu.h"

extern "C" _noreturn
void enter_kernel(uint64_t entry_point) _section(".smp.text");

// Save the entry point address for later MP processor startup
_section(".smp.data") uint64_t smp_entry_point;

static int64_t base_adj;

// .smp section pointers
extern char ___smp_st[];
extern char ___smp_en[];

tchar const *cpu_choose_kernel()
{
    char fw_cfg_tracing[1];

    ssize_t got = qemu_fw_cfg(fw_cfg_tracing, sizeof(fw_cfg_tracing),
                              "opt/com.doug16k.dgos.trace");

    if (got >= 1 && fw_cfg_tracing[0] == '1')
        return TSTR "dgos-kernel-tracing";

    return TSTR "dgos-kernel-generic";
}

_noreturn
static void enter_kernel_initial(uint64_t entry_point)
{
    //uintptr_t vbe_info_vector = vbe_select_mode(65535, 800, 1) << 4;
    //vbe_info_vector = vbe_select_mode(1920, 1080, 1) << 4;

    //
    // Relocate MP entry trampoline to 4KB boundary in the heap

    void *ap_entry_ptr = malloc_aligned(___smp_en - ___smp_st, PAGE_SIZE);

    PRINT("SMP trampoline at 0x%zx:%zx",
          uintptr_t(ap_entry_ptr) >> 4,
          uintptr_t(ap_entry_ptr) & 0xF);

    memcpy(ap_entry_ptr, ___smp_st, ___smp_en - ___smp_st);

    //
    // Build physical memory table

    int phys_mem_table_size = 0;
    void *phys_mem_table = physmap_get(&phys_mem_table_size);

    PRINT("Mapping aliasing window\n");

    // Map first 4GB of physical addresses at -518G
    paging_map_physical(0, -(UINT64_C(518) << 30),
                        UINT64_C(4) << 30,
                        PTE_PRESENT | PTE_WRITABLE | PTE_EX_PHYSICAL);

    // Map a page that the kernel can use to manipulate
    // arbitrary physical addresses by changing its pte
    paging_map_physical(0, (0xFFFFFFFF80000000ULL - PAGE_SIZE) + base_adj,
                     PAGE_SIZE, PTE_PRESENT | PTE_WRITABLE | PTE_EX_PHYSICAL);

    void *heap_st, *heap_en;
    malloc_get_heap_range(&heap_st, &heap_en);
    paging_map_physical(uint64_t(heap_st), uint64_t(heap_st),
                        uint64_t(heap_en) - uint64_t(heap_st),
                        PTE_PRESENT | PTE_WRITABLE);

    kernel_params_t *params = new kernel_params_t{};

    params->size = sizeof(*params);

    params->ap_entry = uintptr_t((void(*)())ap_entry_ptr);
    params->phys_mem_table = uint64_t(phys_mem_table);
    params->phys_mem_table_size = phys_mem_table_size;
    //params.vbe_selected_mode = vbe_info_vector;
    params->boot_drv_serial = boot_serial();

    params->acpi_rsdt = boottbl_find_acpi_rsdp();
    params->mptables = boottbl_find_mptables();

    boot_menu_show(*params);

    PRINT("           ap_entry: 0x%llx\n", uint64_t(params->ap_entry));
    PRINT("     phys_mem_table: 0x%llx\n",
          uint64_t(params->phys_mem_table));
    PRINT("phys_mem_table_size: 0x%llx\n", params->phys_mem_table_size);
    PRINT("  vbe_selected_mode: 0x%llx\n",
               uint64_t(params->vbe_selected_mode));
    PRINT("    boot_drv_serial: 0x%llx\n", params->boot_drv_serial);
    PRINT("    serial_debugout: 0x%llx\n",
               uint64_t(params->serial_debugout));
    PRINT("           wait_gdb: 0x%x\n", params->wait_gdb);

    ELF64_TRACE("Entry point: 0x%llx\n", entry_point);

    PRINT("Entering kernel at 0x%llx\n", entry_point);

    run_kernel(entry_point, params);
}

void enter_kernel(uint64_t entry_point)
{
    if (smp_entry_point == 0) {
        smp_entry_point = entry_point;
        enter_kernel_initial(entry_point);
    } else {
        run_kernel(entry_point, nullptr);
    }
}

void elf64_run(tchar const *filename)
{
    cpu_init();

    uint64_t pge_page_flags = 0;
    if (cpu_has_global_pages())
        pge_page_flags |= PTE_GLOBAL;

    uint64_t nx_page_flags = 0;
    if (cpu_has_no_execute())
        nx_page_flags = PTE_NX;

    // Map the screen for debugging convenience
    paging_map_physical(0xA0000, 0xA0000, 0x20000,
                        PTE_PRESENT | PTE_WRITABLE |
                        (-cpu_has_global_pages() & PTE_GLOBAL) |
                        PTE_PCD | PTE_PWT);

    // Allocate a page of memory to be used to alias high memory
    // Map two pages to simplify copies that are not page aligned
    uintptr_t address_window =
            (uintptr_t)malloc_aligned(PAGE_SIZE << 1, PAGE_SIZE);

    PRINT("Loading %s...\n", filename);

    int file = boot_open(filename);

    if (file < 0)
        PRINT("Could not open kernel file");

    ssize_t read_size;
    Elf64_Ehdr file_hdr;

    read_size = boot_pread(file, &file_hdr, sizeof(file_hdr), 0);

    if (read_size != sizeof(file_hdr))
        PANIC("Could not read ELF header");

    // Check magic number
    if (memcmp(&file_hdr.e_ident[EI_MAG0],
               elf_magic, sizeof(elf_magic)))
        PANIC("Could not read magic number");

    // Load program headers
    Elf64_Phdr *program_hdrs;
    read_size = sizeof(*program_hdrs) * file_hdr.e_phnum;
    program_hdrs = (Elf64_Phdr*)malloc(read_size);
    if (!program_hdrs)
        PANIC("Insufficient memory for program headers");
    if (read_size != boot_pread(
                file,
                program_hdrs,
                read_size,
                file_hdr.e_phoff))
        PANIC("Could not read program headers");

    elf64_context_t *ctx = load_kernel_begin();

    for (unsigned i = 0; i < file_hdr.e_phnum; ++i) {
        if ((program_hdrs[i].p_flags & (PF_R | PF_W | PF_X)) != 0)
            ctx->total_bytes += program_hdrs[i].p_memsz;
    }

    // Load relocations
    if (file_hdr.e_shentsize != sizeof(Elf64_Shdr))
        PRINT("Executable has unexpected section header size");

    ssize_t shbytes = file_hdr.e_shentsize * file_hdr.e_shnum;
    Elf64_Shdr *shdrs = (Elf64_Shdr*)malloc(shbytes);

    if (shbytes != boot_pread(file, shdrs, shbytes, file_hdr.e_shoff))
        PANIC("Could not read section headers\n");

    uint64_t new_base = 0xFFFFFFFF80000000;
    base_adj = new_base - 0xFFFFFFFF80000000;

    PRINT("Loading kernel...");

    // For each program header
    for (size_t i = 0; i < file_hdr.e_phnum; ++i) {
        Elf64_Phdr *blk = program_hdrs + i;

        blk->p_vaddr += base_adj;

        // If it is not readable, writable or executable, ignore
        if ((blk->p_flags & (PF_R | PF_W | PF_X)) != 0) {
            ELF64_TRACE("vaddr=0x%llx, filesz=0x%llx,"
                        " memsz=0x%llx, paddr=0x%llx",
                       blk->p_vaddr, blk->p_filesz,
                       blk->p_memsz, blk->p_paddr);

            if (blk->p_memsz == 0)
                continue;

            ctx->page_flags = (-cpu_has_global_pages() & PTE_GLOBAL);

            // Pages present
            ctx->page_flags |= PTE_PRESENT;

            // Global if possible
            ctx->page_flags |= pge_page_flags;

            // If not executable, mark as no execute
            if ((blk->p_flags & PF_X) == 0)
                ctx->page_flags |= nx_page_flags;

            // Writable
            if ((blk->p_flags & PF_W) != 0)
                ctx->page_flags |= PTE_WRITABLE;

            load_kernel_chunk(blk, file, ctx);
        }

        ctx->done_bytes += blk->p_memsz;

        int percent = 100 * ctx->done_bytes / ctx->total_bytes;

        progress_bar_draw(0, 10, 70, percent);
    }

    load_kernel_end(ctx);
    ctx = nullptr;

    for (size_t i = 0; i < file_hdr.e_shnum; ++i) {
        // If no adjustment, don't bother
        if (!base_adj)
            break;

        if (shdrs[i].sh_type == SHT_RELA) {
            Elf64_Rela *rela = (Elf64_Rela*)malloc(shdrs[i].sh_size);
            size_t relcnt = shdrs[i].sh_size / sizeof(*rela);

            if (ssize_t(shdrs[i].sh_size) != boot_pread(
                        file, rela, shdrs[i].sh_size, shdrs[i].sh_offset))
                PANIC("Could not read relocation section");

            reloc_kernel(base_adj, rela, relcnt);

            free(rela);
        }
    }

    boot_close(file);

    paging_modify_flags(address_window, PAGE_SIZE << 1,
                        ~(uint64_t)0, 0);

    free(program_hdrs);

    // This check is done late to make debugging easier
    // It is impossible to debug 32 bit code on qemu-x86_64 target
    if (!cpu_has_long_mode())
        PANIC("Need 64-bit CPU");

    ELF64_TRACE("Entering kernel");

    enter_kernel(file_hdr.e_entry + base_adj);
}

extern "C" _noreturn
void __cxa_pure_virtual()
{
    PANIC("Pure virtual call!");
}
