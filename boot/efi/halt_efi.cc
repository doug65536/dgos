#include "halt.h"
#include "debug.h"
#include "screen.h"

extern "C" _noreturn void panic(tchar const *s)
{
    //debug_out(s, -1);
    PRINT("%s", s);

    for (;;)
        __asm__ __volatile__ ("hlt");
}
