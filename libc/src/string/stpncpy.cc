#include <string.h>

char *stpncpy(char *restrict lhs, char const *restrict rhs, size_t sz)
{
    auto d = lhs;
    auto s = rhs;
    size_t i;
    for (i = 0; i < sz && (d[i] = s[i]) != 0; ++i);
    char *r = d + i;
    for ( ; i < sz; ++i)
        d[i] = 0;
    return r;
}
