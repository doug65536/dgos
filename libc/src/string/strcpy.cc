#include <string.h>

char *strcpy(char *restrict lhs, char const *restrict rhs)
{
    for (size_t i = 0; (lhs[i] = rhs[i]) != 0; ++i);
    return lhs;
}
