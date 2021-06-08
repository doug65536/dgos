#pragma once
#include "types.h"
#ifndef __DGOS_OFFSET_GENERATOR__
#include "cpu_info.ofs.h"
#endif
#include "segrw.h"

__BEGIN_DECLS

// Call external code with guarantee of no registers affected
// As an added bonus, it also creates a compiler barrier
// Only rax and rflags clobbered
static _always_inline void cs_enter() {
#ifndef __DGOS_OFFSET_GENERATOR__
    cpu_gs_inc<CPU_INFO_LOCKS_HELD_OFS>();
#endif
}

extern "C" void cs_leave_asm();
static _always_inline void cs_leave() {
#ifndef __DGOS_OFFSET_GENERATOR__
    cs_leave_asm();
#endif
}

__END_DECLS
