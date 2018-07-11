#include "control_regs.h"
#include "assert.h"
#include "halt.h"

_noinline
void cpu_debug_break()
{
}

void cpu_debug_breakpoint_set_indirect(uintptr_t addr, int rw,
                                       int len, int enable, size_t index)
{
    typedef void (*handler_t)(uintptr_t addr, int rw, int len, int enable);

    static handler_t constexpr cpu_set_debug_breakpoint_handlers[4] = {
        cpu_debug_breakpoint_set<0>,
        cpu_debug_breakpoint_set<1>,
        cpu_debug_breakpoint_set<2>,
        cpu_debug_breakpoint_set<3>
    };

    cpu_set_debug_breakpoint_handlers[index](addr, rw, len, enable);
}
