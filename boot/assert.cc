#include "assert.h"
#include "screen.h"

_noinline void cpu_debug_break()
{
}

int __assert_failed(tchar const *expr,
                   tchar const *msg,
                   tchar const *file,
                   int_fast32_t line)
{
    PRINT("\n** ASSERT FAILED: "
          "%" TFMT " (%" PRIdFAST32 "): %" TFMT " %" TFMT "\n",
          file, line, expr, msg ? msg : TSTR "");
    cpu_debug_break();
    return 0;
}

