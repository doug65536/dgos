#include <string.h>

size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}
