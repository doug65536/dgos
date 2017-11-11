#include <string.h>

char *strcat(char *restrict lhs, const char *restrict rhs)
{
    return strcpy(lhs + strlen(lhs), rhs);
}
