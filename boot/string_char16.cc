#include "string.h"

size_t strlen(char16_t const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
void *strchr(char16_t const *s, int ch)
{
    for (;; ++s) {
        char c = *s;
        if (c == (char)ch)
            return (void*)s;
        if (c == 0)
            return 0;
    }
}

int strcmp(char16_t const *lhs, char16_t const *rhs)
{
    int cmp = 0;
    do {
        cmp = int((unsigned)*lhs) -
                int((unsigned)*rhs++);
    } while (cmp == 0 && *lhs++);
    return cmp;
}

int strncmp(char16_t const *lhs, char16_t const *rhs, size_t count)
{
    int cmp = 0;
    if (count) {
        do {
            cmp = int((unsigned)*lhs) -
                    int((unsigned)*rhs++);
        } while (--count && cmp == 0 && *lhs++);
    }
    return cmp;
}

char16_t *strcpy(char16_t *dest, char16_t const *src)
{
    char16_t *d = dest;
    while ((*d++ = *src++) != 0);
    return dest;
}

char16_t *strcat(char16_t *dest, char16_t const *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

char16_t *strncpy(char16_t *dest, char16_t const *src, size_t n)
{
    char16_t *d = dest;

    size_t i = 0;

    // Copy from src up to but not including null terminator
    for ( ; i < n && src[i]; ++i)
        d[i] = src[i];

    // Fill dest with zeros until at least n bytes are written
    for ( ; i < n; ++i)
        d[i] = 0;

    return dest;
}

char16_t *strncat(char16_t *dest, char16_t const *src, size_t n)
{
    return strncpy(dest + strlen(dest), src, n);
}
