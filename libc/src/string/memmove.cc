#include <string.h>

void *memmove(void *lhs, void const *rhs, size_t sz)
{
    auto d = (char *)lhs;
    auto s = (char const *)rhs;

    if (d < s || s + sz <= d)
        return memcpy(d, s, sz);

    if (d > s) {
        for (size_t i = sz; i; --i)
            d[i-1] = s[i-1];
    }

    return lhs;
}
