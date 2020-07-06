#include <stdio.h>

int snprintf(char *str, size_t sz, char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(str, sz, format, ap);
    va_end(ap);
    return result;
}
