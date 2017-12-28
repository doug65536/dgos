#include <string.h>
#include <stdlib.h>

char *strndup(char const *s, size_t sz)
{
    size_t len;
    for (len = 0; len < sz; ++len)
        if (s[len] == 0)
            break;
    char *result = (char*)malloc(len + 1);
    memcpy(result, s, len);
    result[len] = 0;
    return result;
}
