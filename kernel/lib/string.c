#include "string.h"

size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

void *memchr(void const *mem, int ch, size_t count)
{
    for (char const *p = mem; count--; ++p)
        if (*p == (char)ch)
            return (void *)p;
   return 0;
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
void *strchr(char const *s, int ch)
{
    for (;; ++s) {
        char c = *s;
        if (c == (char)ch)
            return (void*)s;
        if (c == 0)
            return 0;
    }
}

int strcmp(char const *lhs, char const *rhs)
{
    unsigned char cmp = 0;
    do {
        cmp = (unsigned char)(*lhs) -
                (unsigned char)(*rhs++);
    } while (cmp == 0 && *lhs++);
    return cmp;
}

int strncmp(char const *lhs, char const *rhs, size_t count)
{
    unsigned char cmp = 0;
    if (count) {
        do {
            cmp = (unsigned char)(*lhs) -
                    (unsigned char)(*rhs++);
        } while (--count && cmp == 0 && *lhs++);
    }
    return cmp;
}

int memcmp(void const *lhs, void const *rhs, size_t count)
{
    unsigned char const *lp = lhs;
    unsigned char const *rp = rhs;
    unsigned char cmp = 0;
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

// Returns a pointer to after the last byte written!
void *aligned16_memset(void *dest, int c, size_t n)
{
    char cc = (char)c;
    __ivec16 v = {
        cc, cc, cc, cc, cc, cc, cc, cc,
        cc, cc, cc, cc, cc, cc, cc, cc
    };
    __ivec16 *vp = (__ivec16*)dest;
    while (n >= sizeof(__ivec16)) {
        *vp++ = v;
        n -= sizeof(__ivec16);
    }
    return vp;
}

void *memset(void *dest, int c, size_t n)
{
    char *p = dest;
#ifdef __GNUC__
    // Write bytes until aligned
    while ((intptr_t)p & 0x0F && n) {
        *p++ = (char)c;
        --n;
    }
    // Write as many 128-bit chunks as possible
    if (n > sizeof(__ivec16)) {
        p = aligned16_memset(p, c, n);
        n &= 15;
    }
#endif
    while (n--)
        *p++ = (char)c;
    return dest;
}

void *memcpy(void *dest, void const *src, size_t n)
{
    char *d;
    char const *s;

#ifdef __GNUC__
    // If source and destination are aligned, copy 128 bits at a time
    if (n >= 16 && !((intptr_t)dest & 0x0F) && !((intptr_t)src & 0x0F)) {
        __ivec2 *vd = dest;
        __ivec2 const *vs = src;
        while (n >= sizeof(*vd)) {
            *vd++ = *vs++;
            n -= sizeof(*vd);
        }
        // Do remainder as bytes
        d = (char*)vd;
        s = (char*)vs;
    } else {
        // Not big enough, or not aligned
        d = (char*)dest;
        s = (char*)src;
    }
#else
    d = dest;
    s = src;
#endif

    while (n--)
        *d++ = *s++;

    return dest;
}

void *memmove(void *dest, void const *src, size_t n)
{
    char *d = dest;
    char const *s = src;

    // Can do forward copy if destination is before source,
    // or the end of the source is before the destination
    if (d < s || s + n <= d)
        return memcpy(d, s, n);

    if (d > s) {
        for (size_t i = n; i; --n)
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

char *strcat(char *dest, char const *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

char *strncpy(char *dest, char const *src, size_t n)
{
    char *d = dest;

    size_t i = 0;

    // Copy from src up to and including null terminator
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
