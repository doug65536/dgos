#pragma once
#include "types.h"
#include "asm_constants.h"

__BEGIN_DECLS

// Call external code with guarantee of no registers affected
// As an added bonus, it also creates a compiler barrier
// Registers are not clobbered (but flags are)
static _always_inline void cs_enter() {
    __asm__ __volatile__ (
        "call cs_enter_asm\n\t" : : : "memory", "cc", "rax"
    );
}

extern "C" void cs_leave_asm();
static _always_inline void cs_leave() {
    cs_leave_asm();
}

__END_DECLS
