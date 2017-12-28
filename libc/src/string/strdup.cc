#include <string.h>
#include <stdlib.h>

char *strdup(char const *rhs)
{
    size_t len = strlen(rhs);
    char *buf = (char*)malloc(len + 1);
    return strcpy(buf, rhs);
}
