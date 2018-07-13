#include <string.h>

char *strncpy(char *restrict lhs, char const *restrict rhs, size_t sz)
{
    char *d = lhs;

    size_t i = 0;

    // Copy from src up to but not including null terminator
    for ( ; i < sz && rhs[i]; ++i)
        d[i] = rhs[i];

    // Fill dest with zeros until at least sz bytes are written
    for ( ; i < sz; ++i)
        d[i] = 0;

    return lhs;
}
