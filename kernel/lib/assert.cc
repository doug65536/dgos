#include "assert.h"
#include "printk.h"
#include "cpu/control_regs.h"
#include "cpu/halt.h"

int assert_failed(char const *expr,
                   char const *msg,
                   char const *file,
                   int line)
{
    printdbg("\n** ASSERT FAILED: %s(%d): %s %s\n",
           file, line, expr, msg ? msg : "");
    cpu_debug_break();
    cpu_irq_disable();
    while(true)
        halt();
    return 0;
}
