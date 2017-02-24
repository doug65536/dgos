#include "debug.h"
#include "string.h"

static void e9out(void const *data, size_t len)
{
    __asm__ __volatile__ (
        "rep outsb\n\t"
        "out %%al,%%dx\n\t"
        :
        : "S" (data), "c" (len), "d" (0xe9), "a" ('\n')
        : "memory"
    );
}

void debug_out(char const *s, int len)
{
    if (len < 0)
        len = strlen(s);
    e9out(s, len);
}
