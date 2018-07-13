#include <string.h>

char *strcat(char *restrict lhs, char const *restrict rhs)
{
    return strcpy(lhs + strlen(lhs), rhs);
}
