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
    ELF64_TRACE("constructing new context\n");

    elf64_context_t *ctx = new (std::nothrow) elf64_context_t();
    return ctx;
}

void load_kernel_end(elf64_context_t *ctx)
{
    ELF64_TRACE("deleting context\n");

    delete ctx;
}

bool load_kernel_chunk(Elf64_Phdr *blk, int file, elf64_context_t *ctx)
{
    ELF64_TRACE("loading kernel chunk"
                ", vaddr=0x%" PRIx64
                ", memsz=0x%" PRIx64
                ", page_flags=0x%" PRIx64,
                blk->p_vaddr,
                blk->p_memsz,
                ctx->page_flags);

    alloc_page_factory_t allocator;

    paging_map_range(&allocator, blk->p_vaddr, blk->p_memsz,
                     ctx->page_flags);

    iovec_t *iovec;
    size_t iovec_count = paging_iovec(&iovec, blk->p_vaddr,
                                   blk->p_filesz, 1 << 30);

    uint64_t offset = 0;

    for (size_t i = 0; i < iovec_count; ++i) {
        assert(iovec[i].base < 0x100000000);

        uint64_t file_ofs = blk->p_offset + offset;

        ELF64_TRACE("scatter to paddr=0x%" PRIx64
                    ", size=0x%" PRIx64
                    ", fpos=0x%" PRIx64,
                    iovec[i].base, iovec[i].size, file_ofs);

        ssize_t read = boot_pread(file, (void*)iovec[i].base,
                                  iovec[i].size, file_ofs);

        if (unlikely(read != ssize_t(iovec[i].size)))
            PANIC("Disk error");

        offset += iovec[i].size;
    }

    assert(offset == blk->p_filesz);

    if (blk->p_filesz < blk->p_memsz) {
        free(iovec);
        iovec = nullptr;

        iovec_count = paging_iovec(&iovec, blk->p_vaddr + blk->p_filesz,
                                   blk->p_memsz - blk->p_filesz, 1 << 30);

        for (size_t i = 0; i < iovec_count; ++i) {
            assert(iovec[i].base < 0x100000000);
            memset((void*)iovec[i].base, 0, iovec[i].size);
        }
    }

    free(iovec);
    iovec = nullptr;

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
    kernel_params_t *params = new (std::nothrow) kernel_params_t();

    params->size = sizeof(*params);

    params->ap_entry = uintptr_t((void(*)())ap_entry_ptr);
    params->phys_mem_table = uint64_t(phys_mem_table);
    params->phys_mem_table_size = phys_mem_table_size;
    params->boot_drv_serial = boot_serial();

    params->acpi_rsdt = boottbl_find_acpi_rsdp();
    params->mptables = boottbl_find_mptables();

    // Find NUMA information
    params->numa = boottbl_find_numa(params->acpi_rsdt);

    boot_menu_show(*params);

    // Map framebuffer
    if (params->vbe_selected_mode) {
        auto& mode = *params->vbe_selected_mode;

        vbe_set_mode(mode);

        // Place framebuffer at -2TB
        uint64_t linear_addr = (-UINT64_C(2) << 40);

        PRINT("Mapping framebuffer"
              " vaddr=%" PRIx64
              ", paddr=%" PRIx64
              ", size=%" PRIx64 "\n",
              linear_addr,
              mode.framebuffer_addr,
              mode.framebuffer_bytes);

        paging_map_physical(mode.framebuffer_addr,
                            linear_addr,
                            mode.framebuffer_bytes, PTE_EX_PHYSICAL |
                            PTE_PRESENT | PTE_WRITABLE |
                            PTE_ACCESSED | PTE_DIRTY |
                            PTE_NX | PTE_PTPAT);

        mode.framebuffer_addr = linear_addr;
    }

    return params;
}

_noreturn
static void enter_kernel_initial(uint64_t entry_point, uint64_t base)
{
    //
    // Relocate MP entry trampoline to 4KB boundary in the heap

    size_t smp_sz = ___smp_en - ___smp_st;

    void *ap_entry_ptr = malloc_aligned(smp_sz, PAGE_SIZE);

    memcpy(ap_entry_ptr, ___smp_st, smp_sz);

    //
    // Build physical memory table

    size_t phys_mem_table_size = 0;
    void *phys_mem_table = physmap_get(&phys_mem_table_size);

    uint64_t top_addr =
            physmap_top_addr();

    assert(top_addr != 0);

    // Round up to a 1GB boundary
    top_addr = (top_addr + (UINT64_C(1) << 30) - 1) & -(UINT64_C(1) << 30);

    uint64_t physmap_addr = (base - (UINT64_C(512) << 30)) &
            -(UINT64_C(512) << 30);

    PRINT("Physical mapping, base=%" PRIx64 ", size=%" PRIx64 "\n",
          physmap_addr, top_addr);

    // Map first 512GB of physical addresses
    // at 512GB boundary >= 512GB before load address
    paging_map_physical(0, physmap_addr, top_addr,
                        PTE_PRESENT | PTE_WRITABLE | PTE_EX_PHYSICAL);

    // Map a page that the kernel can use to manipulate
    // arbitrary physical addresses by changing its pte
    paging_map_physical(0, (UINT64_C(0xFFFFFFFF80000000) -
                            PAGE_SIZE) + base_adj,
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

    params->phys_mapping = physmap_addr;
    params->phys_mapping_sz = top_addr;

    // This check is done late to make debugging easier
    // It is impossible to debug 32 bit code on qemu-x86_64 target
    if (unlikely(!cpu_has_long_mode()))
        PANIC("Need 64-bit CPU");

    ELF64_TRACE("Entry point: 0x%llx\n", entry_point);

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

    auto read_size = paging_iovec_read(
                initrd_fd, 0, initrd_base, initrd_size, 1 << 30);
    if (unlikely(initrd_size != read_size))
        PANIC("Could not load initrd file\n");

    boot_close(initrd_fd);
}

void elf64_run(tchar const *filename)
{
    cpu_init();

    uint64_t pge_page_flags = 0;
    if (likely(cpu_has_global_pages()))
        pge_page_flags |= PTE_GLOBAL;

    uint64_t nx_page_flags = 0;
    if (likely(cpu_has_no_execute()))
        nx_page_flags = PTE_NX;

    int file = boot_open(filename);

    if (unlikely(file < 0))
        PANIC("Could not open kernel file");

    ssize_t read_size;
    Elf64_Ehdr file_hdr;

    read_size = boot_pread(file, &file_hdr, sizeof(file_hdr), 0);

    if (unlikely(read_size != sizeof(file_hdr)))
        PANIC("Could not read ELF header");

    //
    // Check magic number and other excuses not to work

    if (unlikely(file_hdr.e_ehsize != sizeof(Elf64_Ehdr)))
        PANIC("Executable has unexpected elf header size");

    if (unlikely(memcmp(&file_hdr.e_ident[EI_MAG0],
                        elf_magic, sizeof(elf_magic))))
        PANIC("Executable has incorrect magic number");

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
                     file, program_hdrs,
                     read_size, file_hdr.e_phoff)))
        PANIC("Could not read program headers");

    elf64_context_t *ctx = load_kernel_begin();

    // Calculate the total bytes to be read for I/O progress bar
    for (unsigned i = 0; i < file_hdr.e_phnum; ++i) {
        if ((program_hdrs[i].p_flags & (PF_R | PF_W | PF_X)) != 0)
            ctx->total_bytes += program_hdrs[i].p_memsz;
    }

    // Load relocations
    ssize_t shbytes = file_hdr.e_shentsize * file_hdr.e_shnum;
    Elf64_Shdr *shdrs = (Elf64_Shdr*)malloc(shbytes);

    if (unlikely(!shdrs))
        PANIC("Insufficient memory for section headers");

    if (unlikely(shbytes != boot_pread(
                     file, shdrs, shbytes, file_hdr.e_shoff)))
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
            ELF64_TRACE("hdr[%zu]: vaddr=0x%" PRIx64
                        ", filesz=0x%" PRIx64
                        ", memsz=0x%" PRIx64 "\n",
                       i, blk->p_vaddr, blk->p_filesz, blk->p_memsz);

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

            if (unlikely(!rela))
                PANIC("Insufficient memory for relocation section");

            size_t relcnt = shdrs[i].sh_size / sizeof(*rela);

            if (unlikely(ssize_t(shdrs[i].sh_size) != boot_pread(
                             file, rela,
                             shdrs[i].sh_size, shdrs[i].sh_offset)))
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
int __cxa_guard_acquire(uint64_t *guard_object)
{
    return *guard_object == 0;
}

// Do nothing to synchronize initialization
// of static variables at function scope
extern "C"
void __cxa_guard_release(uint64_t *guard_object)
{
    *guard_object = 1;
}

