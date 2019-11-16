#pragma once
#include "types.h"
#include "asm_constants.h"
#include "segrw.h"

__BEGIN_DECLS

// Call external code with guarantee of no registers affected
// As an added bonus, it also creates a compiler barrier
// Only rax and rflags clobbered
static _always_inline void cs_enter() {
    cpu_gs_inc<CPU_INFO_LOCKS_HELD_OFS>();
}

extern "C" void cs_leave_asm();
static _always_inline void cs_leave() {
    cs_leave_asm();
}

__END_DECLS
