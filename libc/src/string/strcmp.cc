#include <string.h>

int strcmp(char const *lhs, char const *rhs)
{
    auto a = (unsigned char const *)lhs;
    auto b = (unsigned char const *)rhs;
    int diff = 0;
    for (size_t i = 0; !diff && (b[i] || a[i]); ++i)
        diff = b[i] - a[i];
    return diff;
}
