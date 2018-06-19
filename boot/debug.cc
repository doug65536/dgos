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

void debug_out(char16_t const *s, int len)
{
    if (len < 0)
        len = strlen(s);
    char *tmp = (char*)__builtin_alloca(len+1);
    for (int i = 0; i < len; ++i)
        tmp[i] = char(s[i]);
    e9out(tmp, len);
}
