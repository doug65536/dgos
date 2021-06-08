#include "string.h"
#include "utf.h"
#include "likely.h"
#include "malloc.h"
#include "assert.h"

#if defined(__x86_64__) || defined(__i386__)
#define USE_REP_STRING
#endif

size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
char *strchr(char const *s, int ch)
{
    for (;; ++s) {
        char c = *s;
        if (c == (char)ch)
            return (char*)s;
        if (c == 0)
            return nullptr;
    }
}

int strcmp(char const *lhs, char const *rhs)
{
    int cmp = 0;
    do {
        cmp = int((unsigned)*lhs) -
                int((unsigned)*rhs++);
    } while (cmp == 0 && *lhs++);
    return cmp;
}

int strncmp(char const *lhs, char const *rhs, size_t count)
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

char *strcpy(char *dest, char const *src)
{
    char *d = dest;
    while ((*d++ = *src++) != 0);
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

char *strcat(char *dest, char const *src)
{
    strcpy(dest + strlen(dest), src);
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
char *strncat(char * restrict dest, char const * restrict src, size_t n)
{
    return strncpy(dest + strlen(dest), src, n);
}

void *memchr(void const *mem, int ch, size_t count)
{
    for (char const *p = (char const *)mem; count--; ++p)
        if (*p == (char)ch)
            return (void *)p;
   return nullptr;
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
        return nullptr;

    // Only search as far as substr would fit within str
    size_t chklen = slen - blen;

    // Try each starting point
    for (size_t i = 0; i <= chklen; ++i)
        if (memcmp(str + i, substr, blen) == 0)
            return (char*)(str + i);

    return nullptr;
}

void *memcpy_rev(void *dest, void const *src, size_t n)
{
#ifdef USE_REP_STRING
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
            :
            : "memory"
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
            :
            : "memory"
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

    if (likely(n)) {
        if (d < s || s + n <= d)
            return memcpy(d, s, n);

        if (d != s) {
            for (size_t i = n; i; --i)
                d[i-1] = s[i-1];
        }
    }

    return dest;
}

void *memset(void *dest, int c, size_t n)
{
#ifdef USE_REP_STRING
    char *d = (char*)dest;
    size_t remainder = n & 3;

    n >>= 2;
    __asm__ __volatile__ (
        "cld\n\t"
        "rep stosl\n\t"
        "mov %[remainder],%[count]\n\t"
        "rep stosb\n\t"
        : "+D" (d)
        , [count] "+c" (n)
        : "a" ((c & 0xFF) * 0x01010101)
        , [remainder] "d" (remainder)
        : "memory"
    );
#else
    char *p = (char*)dest;
    while (n--)
        *p++ = (char)c;
#endif
    return dest;
}

#ifdef USE_REP_STRING
void *memcpy(void *dest, void const *src, size_t n)
{
    void *ret = dest;
    size_t remainder = n & 3;
    n >>= 2;
    __asm__ __volatile__ (
        "cld\n\t"
        "rep movsl\n\t"
        "mov %[remainder],%[count]\n\t"
        "rep movsb\n\t"
        : "+D" (dest), "+S" (src), [count] "+c" (n)
        : [remainder] "d" (remainder)
        : "memory"
    );
    return ret;
}
#else
void *memcpy(void *dest, void const *src, size_t n)
{
    char *d = (char*)dest;
    char const *s = (char const *)src;

    while (n--)
        *d++ = *s++;

    return dest;
}
#endif

char *utf8_from_tchar(char *block)
{
    return block;
}

char *utf8_from_tchar(char16_t *block)
{
    if (unlikely(!block))
        return nullptr;

    size_t len = 0;

    char32_t codepoint;

    // Measure the buffer size needed
    for (char16_t const *in = block;
         (codepoint = utf16_to_ucs4_upd(in)) != 0;
         len += ucs4_to_utf8(nullptr, codepoint));

    // Allocate output buffer
    char *result = (char*)malloc(len + 1);

    if (unlikely(!result))
        return nullptr;

    char *out = result;
    if (likely(len)) {
        for (char16_t const *in = block;
             (codepoint = utf16_to_ucs4_upd(in)) != 0;
             out += ucs4_to_utf8(out, codepoint));
    } else {
        *out = 0;
    }

    assert(result + len == out);
    assert(result[len] == 0);

    return result;
}
