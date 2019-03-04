#pragma once
#include "types.h"
#include "asm_constants.h"

__BEGIN_DECLS

// Call external code with guarantee of no registers affected
// As an added bonus, it also creates a compiler barrier
// Allows spinlocks to avoid disabling interrupts
extern _always_inline void cs_enter() {
    __asm__ __volatile__ (
        "call cs_enter_asm\n\t" : : : "memory"
    );
}

// Call external code with guarantee of no registers affected
// As an added bonus, it also creates a compiler barrier
// Allows spinlocks to avoid disabling interrupts
extern _always_inline void cs_leave() {
    __asm__ __volatile__ (
        "call cs_leave_asm\n\t" : : : "memory"
    );
}

__END_DECLS
