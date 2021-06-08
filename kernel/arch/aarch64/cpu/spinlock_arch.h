#pragma once
#include "types.h"
#include "cpu_info.ofs.h"
#include "thread.h"
#include "segrw.h"

__BEGIN_DECLS

// Call external code with guarantee of no registers affected
// As an added bonus, it also creates a compiler barrier
// Only rax and rflags clobbered
static _always_inline void cs_enter() {
    //todo cpu_gs_inc<CPU_INFO_LOCKS_HELD_OFS>();
}

extern "C" void cs_leave_asm();
static _always_inline void cs_leave() {
    //todo cs_leave_asm();
}

__END_DECLS
