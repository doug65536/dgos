#include "string.h"

size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

size_t strlen(char16_t const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

void *memchr(void const *mem, int ch, size_t count)
{
    for (char const *p = (char const *)mem; count--; ++p)
        if (*p == (char)ch)
            return (void *)p;
   return 0;
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
void *strchr(tchar const *s, int ch)
{
    for (;; ++s) {
        char c = *s;
        if (c == (char)ch)
            return (void*)s;
        if (c == 0)
            return 0;
    }
}

int strcmp(tchar const *lhs, char const *rhs)
{
    int cmp = 0;
    do {
        cmp = int((unsigned)*lhs) -
                int((unsigned)*rhs++);
    } while (cmp == 0 && *lhs++);
    return cmp;
}

int strncmp(tchar const *lhs, char const *rhs, size_t count)
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

int memcmp(void const *lhs, void const *rhs, size_t count)
{
    unsigned char const *lp = (unsigned char const *)lhs;
    unsigned char const *rp = (unsigned char const *)rhs;
    int cmp = 0;
    if (count) {
        do {
            cmp = *lp++ - *rp++;
        } while (cmp == 0 && --count);
    }
    return cmp;
}

char *strstr(char const *str, char const *substr)
{
    // If substr is empty string, return str
    if (*substr == 0)
        return (char*)str;

    size_t slen = strlen(str);
    size_t blen = strlen(substr);

    // If substring is longer than string, impossible match
    if (blen > slen)
        return 0;

    // Only search as far as substr would fit within str
    size_t chklen = slen - blen;

    // Try each starting point
    for (size_t i = 0; i <= chklen; ++i)
        if (memcmp(str + i, substr, blen) == 0)
            return (char*)(str + i);

    return 0;
}

void *memset(void *dest, int c, size_t n)
{
#if 1
    char *d = (char*)dest;
    if ((n & 3) == 0) {
        n >>= 2;
        __asm__ __volatile__ (
            "rep stosl\n\t"
            : "+D" (d)
            , "+c" (n)
            : "a" ((c & 0xFF) * 0x01010101)
        );
    } else {
        __asm__ __volatile__ (
            "rep stosb\n\t"
            : "+D" (d)
            , "+c" (n)
            : "a" (c)
        );
    }
#else
    char *p = (char*)dest;
    while (n--)
        *p++ = (char)c;
#endif
    return dest;
}

void *memcpy(void *dest, void const *src, size_t n)
{
#if 1
    char *d = (char*)dest;
    if ((n & 3) == 0) {
        n >>= 2;
        __asm__ __volatile__ (
            "cld\n\t"
            "rep movsl\n\t"
            : "+S" (src)
            , "+D" (d)
            , "+c" (n)
        );
    } else {
        __asm__ __volatile__ (
            "cld\n\t"
            "rep movsb\n\t"
            : "+S" (src)
            , "+D" (d)
            , "+c" (n)
        );
    }
#else
    char *d = (char*)dest;
    char const *s = (char const *)src;
    while (n--)
        *d++ = *s++;
#endif
    return dest;
}

void *memcpy_rev(void *dest, void const *src, size_t n)
{
#if 1
    if ((n & 3) == 0) {
        src = (char*)src + n - 1;
        char *d = (char*)dest + n - 1;
        n >>= 2;
        __asm__ __volatile__ (
            "std\n\t"
            "rep movsl\n\t"
            "cld\n\t"
            : "+S" (src)
            , "+D" (d)
            , "+c" (n)
        );
    } else {
        src = (char*)src + n - 4;
        char *d = (char*)dest + n - 4;
        __asm__ __volatile__ (
            "std\n\t"
            "rep movsb\n\t"
            "cld\n\t"
            : "+S" (src)
            , "+D" (d)
            , "+c" (n)
        );
    }
    return dest;
#else
    char *d = (char*)dest;
    char const *s = (char const *)src;
    while (n--)
        *d++ = *s++;
#endif
    return dest;
}

void *memmove(void *dest, void const *src, size_t n)
{
    char *d = (char*)dest;
    char const *s = (char const *)src;

    if (d < s || s + n <= d)
        return memcpy(d, s, n);

    if (d > s) {
        for (size_t i = n; i; --i)
            d[i-1] = s[i-1];
    }

    return dest;
}

char *strcpy(char *dest, char const *src)
{
    char *d = dest;
    while ((*d++ = *src++) != 0);
    return dest;
}

char16_t *strcpy(char16_t *dest, char16_t const *src)
{
    char16_t *d = dest;
    while ((*d++ = *src++) != 0);
    return dest;
}

char *strcat(char *dest, char const *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

char16_t *strcat(char16_t *dest, char16_t const *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

char *strncpy(char *dest, char const *src, size_t n)
{
    char *d = dest;

    size_t i = 0;

    // Copy from src up to but not including null terminator
    for ( ; i < n && src[i]; ++i)
        d[i] = src[i];

    // Fill dest with zeros until at least n bytes are written
    for ( ; i < n; ++i)
        d[i] = 0;

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

// The behavior is undefined if the destination array
// does not have enough space for the contents of both
// dest and the first count characters of src, plus the
// terminating null character. The behavior is undefined
// if the source and destination objects overlap. The
// behavior is undefined if either dest is not a pointer
// to a null-terminated byte string or src is not a
// pointer to a character array.
char *strncat(char *dest, char const *src, size_t n)
{
    return strncpy(dest + strlen(dest), src, n);
}

char16_t *strncat(char16_t *dest, char16_t const *src, size_t n)
{
    return strncpy(dest + strlen(dest), src, n);
}
