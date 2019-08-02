#include <stdio.h>

int vprintf(char const *format, va_list ap)
{
    return vfprintf(stdout, format, ap);
}
