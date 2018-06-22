#include "halt.h"

extern "C" _noreturn void panic(tchar const *s)
{
    uint16_t *screen = (uint16_t*)0xB8000;
    while (*s)
        *screen++ = (*s++ & 0xFF) | 0xF00;

    for (;;)
        __asm__ __volatile__ ("hlt");
}
