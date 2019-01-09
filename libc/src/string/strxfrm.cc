#include <string.h>

size_t strxfrm(char *restrict dest, char const *restrict src, size_t count)
{
    // Doesn't really transform, just copies the original string
    if (dest)
        strncpy(dest, src, count);

    // Return the length
    return strlen(src);
}
