#include <stdio.h>

int printf(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vfprintf(stdout, format, ap);
    va_end(ap);
    return result;
}
