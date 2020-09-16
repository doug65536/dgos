#pragma once
#include "types.h"
#include "control_regs.h"

__BEGIN_DECLS

_always_inline
static void halt(void)
{
    cpu_halt();
}

extern "C" _noreturn void halt_forever(void);

__END_DECLS
