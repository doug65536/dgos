#include "arch_paging.h"
#include "paging.h"
#include "cpu_x86.h"
#include "cpuid.h"

void arch_set_page_flags(
        elf64_context_t *ctx, intptr_t vaddr,
        bool is_r, bool is_w, bool is_x)
{
    uint64_t pge_page_flags = 0;
    if (likely(arch_has_global_pages() && vaddr < 0))
        pge_page_flags |= PTE_GLOBAL;

    // Global if possible
    ctx->page_flags = pge_page_flags;

    // Pages present
    ctx->page_flags |= -is_r & PTE_PRESENT;

    uint64_t nx_page_flags = 0;
    if (likely(nx_available))
        nx_page_flags = PTE_NX;

    // If not executable, mark as no execute
    ctx->page_flags |= nx_page_flags & -!is_x;

    // Writable
    if (is_w)
        ctx->page_flags |= PTE_WRITABLE;
}


bool arch_has_no_execute()
{
    return true;
}

bool arch_has_global_pages()
{
    return cpu_has_global_pages();
}
