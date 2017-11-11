#include <string.h>

void *memccpy(void *restrict lhs, const void *restrict rhs, int c, size_t sz)
{
    auto d = (char *)lhs;
    auto s = (char const *)rhs;
    auto cc = char(c);
    for (size_t i = 0; i < sz; ++i) {
        char x = s[i];
        d[i] = x;
        if (x == cc)
            return d + i + 1;
    }
    return nullptr;
}
