#include <string.h>

char *stpcpy(char *restrict lhs, const char *restrict rhs)
{
    auto d = lhs;
    auto s = rhs;
    size_t i;
    for (i = 0; (d[i] = s[i]) != 0; ++i);
    return d + i;
}
