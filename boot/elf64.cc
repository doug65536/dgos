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
#include "messagebar.h"
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

    elf64_context_t *ctx = new (ext::nothrow) elf64_context_t();
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
        case 'A': return TSTR "boot/dgos-kernel-asan";
        case 'T': return TSTR "boot/dgos-kernel-tracing";
        }
    }
    return TSTR "boot/dgos-kernel-generic";
}

static kernel_params_t *prompt_kernel_param(
        void *phys_mem_table, void *ap_entry_ptr, int phys_mem_table_size)
{
    kernel_params_t *params = new (ext::nothrow) kernel_params_t();

    PRINT("Preparing kernel parameter structure at %p\n", (void*)params);

    params->size = sizeof(*params);

    params->ap_entry = uintptr_t((void(*)())ap_entry_ptr);
    params->phys_mem_table = uint64_t(phys_mem_table);
    params->phys_mem_table_size = phys_mem_table_size;
    params->boot_drv_serial = boot_serial();

    params->acpi_rsdt = boottbl_find_acpi_rsdp();
    params->mptables = boottbl_find_mptables();

    // Find NUMA information
    params->numa = boottbl_find_numa(params->acpi_rsdt);

    PRINT("Showing boot menu");

    boot_menu_show(*params);

    PRINT("Done boot menu");

    // Map framebuffer
    if (params->vbe_selected_mode) {
        auto& mode = *params->vbe_selected_mode;

        vbe_set_mode(mode);

        PRINT("Got framebuffer"
              ", paddr=%" PRIx64
              ", size=%" PRIx64 "\n",
              mode.framebuffer_addr,
              mode.framebuffer_bytes);

//        paging_map_physical(mode.framebuffer_addr,
//                            linear_addr,
//                            mode.framebuffer_bytes, PTE_EX_PHYSICAL |
//                            PTE_PRESENT | PTE_WRITABLE |
//                            PTE_ACCESSED | PTE_DIRTY |
//                            PTE_NX | PTE_PTPAT);

        //mode.framebuffer_addr = linear_addr;
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

    uint64_t top_addr = physmap_top_addr();

    assert(top_addr != 0);

    // Round up to a 1GB boundary
    top_addr = (top_addr + (UINT64_C(1) << 30) - 1) & -(UINT64_C(1) << 30);

    // 512GB below base of kernel, rounded down to 1GB boundary
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
    paging_map_physical(0, (base - PAGE_SIZE) + base_adj,
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

    PRINT("Bootloader entering kernel");

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

void progress(size_t fsz, size_t msz, elf64_context_t *ctx)
{
    int64_t now = systime();

    ctx->done_file_bytes += fsz;
    ctx->done_mem_bytes += msz;

    _assume(ctx->done_file_bytes < ctx->total_file_bytes);

    int file_percent = int(UINT64_C(100) *
                           ctx->done_file_bytes / ctx->total_file_bytes);
    int mem_percent = int(UINT64_C(100) *
                           ctx->done_mem_bytes / ctx->total_mem_bytes);

    progress_bar_draw(17, 0, 63, file_percent, TSTR "I/O", file_percent > 0);

    progress_bar_draw(17, 3, 63, mem_percent, TSTR "RAM", mem_percent > 0);

    print_line_at(65, 1, 0x7, TSTR "%" PRIu64 "/%" PRIu64 "MB",
                  ctx->done_file_bytes >> 20,
                  ctx->total_file_bytes >> 20);

    print_line_at(65, 4, 0x7, TSTR "%" PRIu64 "/%" PRIu64 "MB",
                  ctx->done_mem_bytes >> 20,
                  ctx->total_mem_bytes >> 20);

    int64_t elap = 0;
    if ((ctx->last_time && now > ctx->last_time) || file_percent == 100) {
        elap = now - ctx->last_time;

        int64_t delta, ms;
        if (file_percent < 100) {
            delta = ctx->done_file_bytes - ctx->last_file_bytes;
            ctx->last_file_bytes = ctx->done_file_bytes;
        } else {
            elap = now - ctx->start_time;
            delta = ctx->done_file_bytes;
        }

        ms = (elap * 10000) / 182;
        assert(ms);
        int64_t kps = ms ? delta / ms : INT64_MAX;

        print_line_at(65, 5, 0x7, TSTR "%" PRId64 "KB/s", kps);
    }
    ctx->last_time = now;
}

void load_initrd(int initrd_fd, off_t initrd_filesize, elf64_context_t *ctx)
{
    ELF64_TRACE("Loading initrd...");

    // load initrd at -64TB
    initrd_base = -(UINT64_C(64) << 40);

    alloc_page_factory_t allocator;

    paging_map_range(&allocator, initrd_base, initrd_size,
                     PTE_PRESENT | PTE_ACCESSED | PTE_NX);

    constexpr off_t sz2M = 1 << 21;
    for (off_t blk_end, ofs = 0; ofs < initrd_size; ofs = blk_end) {
        blk_end = ofs + sz2M;
        off_t blk_size = (blk_end < initrd_size
                          ? blk_end
                          : initrd_size) - ofs;

        auto read_size = paging_iovec_read(
                    initrd_fd, ofs, initrd_base + ofs, blk_size, 1 << 30);

        if (unlikely(read_size != blk_size))
            PANIC("Could not load initrd file\n");

        progress(blk_size, blk_size, ctx);
    }
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

    message_bar_draw(10, 7, 70, TSTR "Getting initrd size");

    int initrd_fd = boot_open(TSTR "boot/initrd");
    initrd_size = boot_filesize(initrd_fd);
    boot_close(initrd_fd);
    initrd_fd = -1;

    message_bar_draw(10, 7, 70, TSTR "Opening kernel image");

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

    if (unlikely(file_hdr.e_machine != EM_AMD64))
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
    for (size_t i = 0; i < file_hdr.e_phnum; ++i) {
        if ((program_hdrs[i].p_flags & (PF_R | PF_W | PF_X)) != 0) {
            ctx->total_file_bytes += program_hdrs[i].p_filesz;
            ctx->total_mem_bytes += program_hdrs[i].p_memsz;
        }
    }

    ctx->total_file_bytes += initrd_size;
    ctx->total_mem_bytes += initrd_size;

    PRINT("Expecting to load %s%" PRIu64 "KiB",
          (ctx->total_file_bytes & ~-(1 << 10)) ? "~" : "",
          ctx->total_file_bytes >> 10);
    PRINT("Expecting to initialize %s%" PRIu64 "KiB",
          (ctx->total_mem_bytes & ~-(1 << 10)) ? "~" : "",
          ctx->total_mem_bytes >> 10);

    // Load relocations
    ssize_t shbytes = file_hdr.e_shentsize * file_hdr.e_shnum;
    Elf64_Shdr *shdrs = (Elf64_Shdr*)malloc(shbytes);

    if (unlikely(!shdrs))
        PANIC("Insufficient memory for section headers");

    if (unlikely(shbytes != boot_pread(
                     file, shdrs, shbytes, file_hdr.e_shoff)))
        PANIC("Could not read section headers\n");

    // OOM if relocated!
    uint64_t new_base = 0xFFFFFFFF80000000; // - 0x000000ff80000000;
    base_adj = new_base - 0xFFFFFFFF80000000;

    message_bar_draw(10, 7, 70, TSTR "Loading kernel");

    progress(0, 0, ctx);

    ctx->start_time = systime();

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

        progress(blk->p_filesz, blk->p_memsz, ctx);
    }

    message_bar_draw(10, 7, 70, TSTR "Applying relocations");

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

    message_bar_draw(10, 7, 70, TSTR "Loading initrd");

    assert(initrd_fd == -1);
    initrd_fd = boot_open(TSTR "boot/initrd");

    load_initrd(initrd_fd, initrd_size, ctx);
    boot_close(initrd_fd);

    load_kernel_end(ctx);

    ctx = nullptr;

    ELF64_TRACE("Entering kernel");

    message_bar_draw(10, 7, 70, TSTR "Showing boot menu");

    enter_kernel(file_hdr.e_entry + base_adj, new_base);
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

