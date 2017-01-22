#include "assert.h"
#include "printk.h"

void assert_failed(char const *expr,
                   char const *file,
                   int line)
{
    printdbg("\n** ASSERT FAILED: %s(%d): %s\n",
           file, line, expr);
}
