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
void enter_kernel(uint64_t entry_point, uint64_t base) _section(".smp.text");

// Save the entry point address for later MP processor startup
uint64_t smp_entry_point _section(".smp.data");

static int64_t base_adj;

static int64_t initrd_base;
static int64_t initrd_size;

// .smp section pointers
extern char ___smp_st[];
extern char ___smp_en[];


elf64_context_t *load_kernel_begin()
{
    elf64_context_t *ctx = new elf64_context_t{};
    return ctx;
}

void load_kernel_end(elf64_context_t *ctx)
{
    delete ctx;
}

bool load_kernel_chunk(Elf64_Phdr *blk, int file, elf64_context_t *ctx)
{
    alloc_page_factory_t allocator;

    paging_map_range(&allocator, blk->p_vaddr, blk->p_memsz,
                     ctx->page_flags);

    iovec_t *iovec;
    int iovec_count = paging_iovec(&iovec, blk->p_vaddr,
                                   blk->p_filesz, 1 << 30);

    uint64_t offset = 0;
    for (int i = 0; i < iovec_count; ++i) {
        assert(iovec[i].base < 0x100000000);

        ssize_t read = boot_pread(
                    file, (void*)iovec[i].base, iovec[i].size,
                    blk->p_offset + offset);

        if (unlikely(read != ssize_t(iovec[i].size)))
            PANIC("Disk error");

        offset += iovec[i].size;
    }

    if (offset < blk->p_memsz) {
        free(iovec);
        iovec_count = paging_iovec(&iovec, blk->p_vaddr + blk->p_filesz,
                                   blk->p_memsz - offset, 1 << 30);
        for (int i = 0; i < iovec_count; ++i) {
            assert(iovec[i].base < 0x100000000);
            memset((void*)iovec[i].base, 0, iovec[i].size);
        }
    }

    free(iovec);

    // Clear modified bits if uninitialized data
    if (blk->p_memsz > blk->p_filesz) {
        paging_modify_flags(blk->p_vaddr + blk->p_filesz,
                            blk->p_memsz - blk->p_filesz,
                            PTE_DIRTY | PTE_ACCESSED, 0);
    }

    return true;
}

// ===========================================================================

tchar const *cpu_choose_kernel()
{
    char fw_cfg_type[1] = { '?' };

    uint32_t file_size = 0;
    int selector = qemu_selector_by_name(
                "opt/com.doug16k.dgos.kernel_type", &file_size);
    if (file_size > 0 && selector >= 0 &&
            qemu_fw_cfg(fw_cfg_type, sizeof(fw_cfg_type),
                        file_size, selector) > 0) {
        switch (fw_cfg_type[0]) {
        case 'A': return TSTR "dgos-kernel-asan";
        case 'T': return TSTR "dgos-kernel-tracing";
        }
    }
    return TSTR "dgos-kernel-generic";
}

static kernel_params_t *prompt_kernel_param(
        void *phys_mem_table, void *ap_entry_ptr, int phys_mem_table_size)
{
    kernel_params_t *params = new kernel_params_t{};

    params->size = sizeof(*params);

    params->ap_entry = uintptr_t((void(*)())ap_entry_ptr);
    params->phys_mem_table = uint64_t(phys_mem_table);
    params->phys_mem_table_size = phys_mem_table_size;
    params->boot_drv_serial = boot_serial();

    params->acpi_rsdt = boottbl_find_acpi_rsdp();
    params->mptables = boottbl_find_mptables();

    boot_menu_show(*params);

    // Map framebuffer
    if (params->vbe_selected_mode) {
        auto& mode = *params->vbe_selected_mode;

        vbe_set_mode(mode);

        // Place framebuffer at -2TB
        uint64_t linear_addr = (-UINT64_C(2) << 40);

        paging_map_physical(mode.framebuffer_addr,
                            linear_addr,
                            mode.framebuffer_bytes, PTE_EX_PHYSICAL |
                            PTE_PRESENT | PTE_WRITABLE |
                            PTE_ACCESSED | PTE_DIRTY |
                            PTE_NX | PTE_PTPAT);

        mode.framebuffer_addr = linear_addr;
    }

//    PRINT("           ap_entry: 0x%llx\n", uint64_t(params->ap_entry));
//    PRINT("     phys_mem_table: 0x%llx\n",
//          uint64_t(params->phys_mem_table));
//    PRINT("phys_mem_table_size: 0x%llx\n", params->phys_mem_table_size);
//    PRINT("  vbe_selected_mode: 0x%llx\n",
//               uint64_t(params->vbe_selected_mode));
//    PRINT("    boot_drv_serial: 0x%llx\n", params->boot_drv_serial);
//    PRINT("    serial_debugout: 0x%llx\n",
//               uint64_t(params->serial_debugout));
//    PRINT("           wait_gdb: 0x%x\n", params->wait_gdb);

    return params;
}

_noreturn
static void enter_kernel_initial(uint64_t entry_point, uint64_t base)
{
    //
    // Relocate MP entry trampoline to 4KB boundary in the heap

    void *ap_entry_ptr = malloc_aligned(___smp_en - ___smp_st, PAGE_SIZE);

    //PRINT("SMP trampoline at 0x%zx:%zx",
    //      uintptr_t(ap_entry_ptr) >> 4,
    //      uintptr_t(ap_entry_ptr) & 0xF);

    memcpy(ap_entry_ptr, ___smp_st, ___smp_en - ___smp_st);

    //
    // Build physical memory table

    int phys_mem_table_size = 0;
    void *phys_mem_table = physmap_get(&phys_mem_table_size);

    //PRINT("Mapping low memory\n");

    uint64_t physmap_addr = (base - (UINT64_C(512) << 30)) &
            -(UINT64_C(512) << 30);

    // Map first 512GB of physical addresses
    // at 512GB boundary >= 512GB before load address
    paging_map_physical(0, physmap_addr,
                        UINT64_C(512) << 30,
                        PTE_PRESENT | PTE_WRITABLE | PTE_EX_PHYSICAL);

    //PRINT("Mapping dynamic frame\n");

    // Map a page that the kernel can use to manipulate
    // arbitrary physical addresses by changing its pte
    paging_map_physical(0, (0xFFFFFFFF80000000ULL - PAGE_SIZE) + base_adj,
                        PAGE_SIZE, PTE_PRESENT |
                        PTE_WRITABLE | PTE_EX_PHYSICAL);

    // Guarantee that the bootloader heap is mapped
    void *heap_st, *heap_en;
    malloc_get_heap_range(&heap_st, &heap_en);
    paging_map_physical(uint64_t(heap_st), uint64_t(heap_st),
                        uint64_t(heap_en) - uint64_t(heap_st),
                        PTE_PRESENT | PTE_WRITABLE | PTE_EX_PHYSICAL);

    kernel_params_t *params = prompt_kernel_param(
                phys_mem_table, ap_entry_ptr, phys_mem_table_size);

    params->initrd_st = initrd_base;
    params->initrd_sz = initrd_size;

    // This check is done late to make debugging easier
    // It is impossible to debug 32 bit code on qemu-x86_64 target
    if (!cpu_has_long_mode())
        PANIC("Need 64-bit CPU");

    ELF64_TRACE("Entry point: 0x%llx\n", entry_point);

    //PRINT("Entering kernel at 0x%llx\n", entry_point);

    run_kernel(entry_point, params);
}

void enter_kernel(uint64_t entry_point, uint64_t base)
{
    if (smp_entry_point == 0) {
        smp_entry_point = entry_point;
        enter_kernel_initial(entry_point, base);
    } else {
        run_kernel(entry_point, nullptr);
    }
}

void load_initrd()
{
    ELF64_TRACE("Loading initrd...");

    int initrd_fd = boot_open(TSTR "initrd");

    initrd_size = boot_filesize(initrd_fd);

    // load initrd at -64TB
    initrd_base = -(UINT64_C(64) << 40);

    alloc_page_factory_t allocator;

    paging_map_range(&allocator, initrd_base, initrd_size,
                     PTE_PRESENT | PTE_ACCESSED | PTE_NX);

    if (initrd_size != paging_iovec_read(
                initrd_fd, 0, initrd_base, initrd_size, 1 << 30)) {
        PANIC("Could not load initrd file\n");
    }

    boot_close(initrd_fd);
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

    //PRINT("Loading %s...\n", filename);

    int file = boot_open(filename);

    if (file < 0)
        PANIC("Could not open kernel file");

    ssize_t read_size;
    Elf64_Ehdr file_hdr;

    read_size = boot_pread(file, &file_hdr, sizeof(file_hdr), 0);

    if (read_size != sizeof(file_hdr))
        PANIC("Could not read ELF header");

    //
    // Check magic number and other excuses not to work

    if (unlikely(file_hdr.e_ehsize != sizeof(Elf64_Ehdr)))
        PANIC("Executable has unexpected elf header size");

    if (unlikely(memcmp(&file_hdr.e_ident[EI_MAG0],
                        elf_magic, sizeof(elf_magic))))
        PANIC("Could not read magic number");

    if (unlikely(file_hdr.e_phentsize != sizeof(Elf64_Phdr)))
        PANIC("Executable has unexpected program header size");

    if (unlikely(file_hdr.e_shentsize != sizeof(Elf64_Shdr)))
        PANIC("Executable has unexpected section header size");

    if (unlikely(file_hdr.e_machine != 62))
        PANIC("Executable is for an unexpected machine id");

    // Load program headers
    Elf64_Phdr *program_hdrs;
    read_size = sizeof(*program_hdrs) * file_hdr.e_phnum;
    program_hdrs = (Elf64_Phdr*)malloc(read_size);
    if (unlikely(!program_hdrs))
        PANIC("Insufficient memory for program headers");
    if (unlikely(read_size != boot_pread(
                     file,
                     program_hdrs,
                     read_size,
                     file_hdr.e_phoff)))
        PANIC("Could not read program headers");

    elf64_context_t *ctx = load_kernel_begin();

    for (unsigned i = 0; i < file_hdr.e_phnum; ++i) {
        if ((program_hdrs[i].p_flags & (PF_R | PF_W | PF_X)) != 0)
            ctx->total_bytes += program_hdrs[i].p_memsz;
    }

    // Load relocations
    ssize_t shbytes = file_hdr.e_shentsize * file_hdr.e_shnum;
    Elf64_Shdr *shdrs = (Elf64_Shdr*)malloc(shbytes);

    if (shbytes != boot_pread(file, shdrs, shbytes, file_hdr.e_shoff))
        PANIC("Could not read section headers\n");

    uint64_t new_base = 0xFFFFFFFF80000000;
    base_adj = new_base - 0xFFFFFFFF80000000;

    //PRINT("Loading kernel...");

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

            // Global if possible
            ctx->page_flags = pge_page_flags;

            // Pages present
            ctx->page_flags |= PTE_PRESENT;

            // If not executable, mark as no execute
            if ((blk->p_flags & PF_X) == 0)
                ctx->page_flags |= nx_page_flags;

            // Writable
            if ((blk->p_flags & PF_W) != 0)
                ctx->page_flags |= PTE_WRITABLE;

            load_kernel_chunk(blk, file, ctx);
        }

        ctx->done_bytes += blk->p_memsz;

        int percent = int(UINT64_C(100) * ctx->done_bytes / ctx->total_bytes);

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

    free(program_hdrs);

    load_initrd();

    ELF64_TRACE("Entering kernel");

    enter_kernel(file_hdr.e_entry + base_adj, 0xFFFFFFFF80000000 + base_adj);
}

extern "C" _noreturn
void __cxa_pure_virtual()
{
    PANIC("Pure virtual call!");
}

// Do nothing to synchronize initialization
// of static variables at function scope
extern "C"
void __cxa_guard_acquire()
{
}

// Do nothing to synchronize initialization
// of static variables at function scope
extern "C"
void __cxa_guard_release()
{
}

