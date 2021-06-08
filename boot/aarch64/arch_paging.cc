#include "arch_paging.h"

void arch_set_page_flags(
        elf64_context_t *ctx, intptr_t vaddr,
        bool is_r, bool is_w, bool is_x)
{
    uintptr_t ap = 0;
    uintptr_t xn = 0;

    xn = !is_x;
    ap = is_w ? 0b01 : 0b11;

    uintptr_t mask = -is_r;

    ctx->page_flags = PTE_XN_n(xn) | PTE_AP_n(ap);
}

bool arch_has_no_execute()
{
    return true;
}

bool arch_has_global_pages()
{
    return true;
}
