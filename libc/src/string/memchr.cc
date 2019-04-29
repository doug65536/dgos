#include <string.h>

void *memchr(void const *dest, int c, size_t sz)
{
    char const *d = (char const *)dest;
    char const cc = (char)c;
    for (size_t i = 0; i < sz; ++i) {
        if (d[i] == cc)
            return (void*)(d + i);
    }
    return nullptr;
}
