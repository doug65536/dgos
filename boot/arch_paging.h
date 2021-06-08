#pragma once

#include "types.h"
#include "elf64_abstract.h"

__BEGIN_DECLS

void arch_set_page_flags(
        elf64_context_t *ctx, intptr_t vaddr,
        bool is_r, bool is_w, bool is_x);

_pure bool arch_has_no_execute();
_pure bool arch_has_global_pages();

__END_DECLS
