#include <string.h>

char *strrchr(char const *s, int c)
{
    unsigned char const *us = (unsigned char const *)s;
    unsigned char uc = (unsigned char)c;
    unsigned char const *r = nullptr;
    for (size_t i = 0; ; ++i) {
        if (us[i] == uc)
            r = (us + i);
        if (us[i] == 0)
            break;
    }

    return (char*)r;
}
