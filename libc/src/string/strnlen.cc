#include <string.h>

size_t strnlen(const char *s, size_t sz)
{
    void const *t = memchr(s, 0, sz);
    return t ? (char*)t - (char*)s : sz;
}
