#include <string.h>

void *memcpy(void *restrict lhs, const void *restrict rhs, size_t sz)
{
    auto d = (char *)lhs;
    auto s = (char const *)rhs;
    for (size_t i = 0; i < sz; ++i)
        d[i] = s[i];
    return d;
}
