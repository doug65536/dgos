#include <string.h>

char *stpcpy(char *restrict lhs, const char *restrict rhs)
{
    auto d = (char *)lhs;
    auto s = (char const *)rhs;
    size_t i;
    for (i = 0; (d[i] = s[i]) != 0; ++i);
    return d + i;
}
