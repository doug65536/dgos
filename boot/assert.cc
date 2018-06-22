#include "assert.h"
#include "screen.h"

_noinline void cpu_debug_break()
{
}

int assert_failed(tchar const *expr,
                   tchar const *msg,
                   tchar const *file,
                   int line)
{
    PRINT(TSTR "\n** ASSERT FAILED: %s(%d): %s %s\n",
          file, line, expr, msg ? msg : TSTR "");
    cpu_debug_break();
    return 0;
}

