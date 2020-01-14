#include "assert.h"
#include "printk.h"
#include "cpu/control_regs.h"
#include "cpu/halt.h"
#include "export.h"
#include "debug.h"

EXPORT int __assert_failed(char const *expr,
                         char const *msg,
                         char const *file,
                         int line)
{
    printdbg("\n** ASSERT FAILED: %s(%d): %s %s\n",
             file, line, expr, msg ? msg : "");
    cpu_debug_break();
    //cpu_irq_disable();
    // volatile because I might change it in the debugger and I want control
    // flow to appear to be able to return and continue
    bool volatile keep_waiting = false;
    while(keep_waiting)
        halt();
    return 0;
}
