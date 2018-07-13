#include <string.h>

int strcoll(char const *lhs, char const *rhs)
{
    // FIXME: implement locales
    return strcmp(lhs, rhs);
}
