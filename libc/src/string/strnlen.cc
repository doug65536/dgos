#include <string.h>

size_t strnlen(char const *s, size_t sz)
{
    void const *t = memchr(s, 0, sz);
    return t ? (char*)t - (char*)s : sz;
}
