#include <string.h>

int strcoll(const char *lhs, const char *rhs)
{
    // FIXME: implement locales
    return strcmp(lhs, rhs);
}
