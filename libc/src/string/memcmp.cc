#include <string.h>

int memcmp(void const *lhs, void const *rhs, size_t sz)
{
    auto d = (unsigned char const *)lhs;
    auto s = (unsigned char const *)rhs;
    int diff = 0;
    for (size_t i = 0; i < sz && !diff; ++i)
        diff = d[i] - s[i];
    return diff;
}
