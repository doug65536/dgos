#include <string.h>

char *strchr(const char *s, int c)
{
    auto cc = char(c);
    size_t i;
    for (i = 0; s[i] && s[i] != cc; ++i);
    return const_cast<char*>(s[i] == cc ? s + i : nullptr);
}
