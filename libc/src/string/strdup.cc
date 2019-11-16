#include <string.h>
#include <stdlib.h>
#include <sys/likely.h>

char *strdup(char const *rhs)
{
    size_t len = strlen(rhs);
    char *buf = (char*)malloc(len + 1);
    if (likely(buf))
        return strcpy(buf, rhs);
    return nullptr;
}
