#include <stdio.h>

int vprintf(const char *format, va_list ap)
{
    return vfprintf(stdout, format, ap);
}
