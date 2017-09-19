#include "control_regs.h"
#include "assert.h"

__noinline
void cpu_debug_break()
{
}

void cpu_set_debug_breakpoint_indirect(uintptr_t addr, int rw,
                                       int len, int enable, size_t index)
{
    typedef void (*handler_t)(uintptr_t addr, int rw, int len, int enable);

    static handler_t constexpr cpu_set_debug_breakpoint_handlers[4] = {
        cpu_set_debug_breakpoint<0>,
        cpu_set_debug_breakpoint<1>,
        cpu_set_debug_breakpoint<2>,
        cpu_set_debug_breakpoint<3>
    };

    cpu_set_debug_breakpoint_handlers[index](addr, rw, len, enable);
}
