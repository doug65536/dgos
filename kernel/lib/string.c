#define __NO_STRING_BUILTIN
#include "string.h"
#undef __NO_STRING_BUILTIN
#include "export.h"
#include "bswap.h"
#include "assert.h"
#include "cpu/memcpy.h"

#define USE_REP_STRING 1

EXPORT size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

EXPORT void *memchr(void const *mem, int ch, size_t count)
{
    for (char const *p = mem; count--; ++p)
        if (*p == (char)ch)
            return (void *)p;
   return 0;
}

EXPORT void *memrchr(const void *mem, int ch, size_t count)
{
    for (char const *p = (char const *)mem + count; count--; --p)
        if (p[-1] == (char)ch)
            return (void*)(p - 1);
   return 0;
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
EXPORT char *strchr(char const *s, int ch)
{
    for (;; ++s) {
        char c = *s;
        if (c == (char)ch)
            return (void*)s;
        if (c == 0)
            return 0;
    }
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
EXPORT char *strrchr(char const *s, int ch)
{
    for (char const *p = s + strlen(s); p >= s; --p) {
        if ((char)*p == (char)ch)
            return (char*)p;
    }
    return 0;
}

EXPORT int strcmp(char const *lhs, char const *rhs)
{
    int cmp = 0;
    do {
        cmp = (unsigned char)(*lhs) -
                (unsigned char)(*rhs++);
    } while (cmp == 0 && *lhs++);
    return cmp;
}

EXPORT int strncmp(char const *lhs, char const *rhs, size_t count)
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

EXPORT int memcmp(void const *lhs, void const *rhs, size_t count)
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

EXPORT char *strstr(char const *str, char const *substr)
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
EXPORT void *aligned16_memset(void *dest, int c, size_t n)
{
#if USE_REP_STRING
    return memset(dest, c, n);
#else
#ifdef __OPTIMIZE__
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
#else
    return (char*)memset(dest, c, n) + n;
#endif
#endif
}

EXPORT void *memset(void *dest, int c, size_t n)
{
    assert(n < 0x0000700000000000L);
#ifdef USE_REP_STRING
    char *d = dest;
    __asm__ __volatile__ (
        "rep stosb\n\t"
        : "+D" (d), "+c" (n)
        : "a" ((uint8_t)c)
        : "memory"
    );
#else
    char *p = dest;
#if defined(__GNUC__) && defined(__OPTIMIZE__)
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
#endif

    return dest;
}

EXPORT void *memcpy(void *dest, void const *src, size_t n)
{
#ifdef USE_REP_STRING
    char *d = dest;
    __asm__ __volatile__ (
        "rep movsb\n\t"
        : "+D" (d), "+S" (src), "+c" (n)
        :
        : "memory"
    );
#else
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
#endif

    return dest;
}

static inline void memcpy_reverse(void *dest, void const *src, size_t n)
{
#if USE_REP_STRING
    __asm__ __volatile__ (
        "std\n\t"
        "rep movsb\n\t"
        "cld\n\t"
        : "+D" (dest), "+S" (src), "+c" (n)
        :
        : "memory"
    );
#else
    for (size_t i = n; i; --i)
        dest[i-1] = src[i-1];
#endif
}

EXPORT void *memmove(void *dest, void const *src, size_t n)
{
    char *d = dest;
    char const *s = src;

    // Can do forward copy if destination is before source,
    // or the end of the source is before the destination
    if (d < s || s + n <= d)
        return memcpy(d, s, n);

    if (d > s)
        memcpy_reverse(d + n - 1, s + n - 1, n);

    return dest;
}

EXPORT char *strcpy(char *dest, char const *src)
{
    char *d = dest;
    while ((*d++ = *src++) != 0);
    return dest;
}

EXPORT char *strcat(char *dest, char const *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

EXPORT char *strncpy(char *dest, char const *src, size_t n)
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
EXPORT char *strncat(char *dest, char const *src, size_t n)
{
    return strncpy(dest + strlen(dest), src, n);
}

// out should have room for at least 5 bytes
// if out is null, returns how many bytes it
// would have wrote to out, not including null terminator
// Returns 0 for values outside 0 <= in < 0x101000 range
// Always writes null terminator if out is not null
EXPORT int ucs4_to_utf8(char *out, int in)
{
    int len;
    if (in >= 0 && in < 0x80) {
        if (out) {
            *out++ = (char)in;
            *out++ = 0;
        }
        return 1;
    }

    if (in < 0x80) {
        len = 2;
    } else if (in < 0x800) {
        len = 3;
    } else if (in < 0x10000) {
        len = 4;
    } else {
        // Invalid
        if (out)
            *out++ = 0;
        return 0;
    }

    if (out) {
        int shift = len - 1;

        *out++ = (char)((signed char)0x80 >> shift) |
                (in >> (6 * shift));

        while (--shift >= 0)
            *out++ = 0x80 | ((in >> (6 * shift)) & 0x3F);

        *out++ = 0;
    }

    return len;
}

EXPORT int ucs4_to_utf16(uint16_t *out, int in)
{
    if ((in > 0 && in < 0xD800) ||
            (in > 0xDFFF && in < 0x10000)) {
        if (out) {
            *out++ = (uint16_t)in;
            *out = 0;
        }
        return 1;
    } else if (in > 0xFFFF && in < 0x110000) {
        in -= 0x10000;
        if (out) {
            *out++ = 0xD800 + ((in >> 10) & 0x3FF);
            *out++ = 0xDC00 + (in & 0x3FF);
            *out = 0;
        }
        return 2;
    }

    // Codepoint out of range or in surrogate range
    if (out)
        *out = 0;

    return 0;
}

// Returns 32 bit wide character
// Returns -1 on error
// If ret_end is not null, pointer to first
// byte after encoded character to *ret_end
// If the input is a null byte, and ret_end is not null,
// then *ret_end is set to point to the null byte, it
// is not advanced
EXPORT int utf8_to_ucs4(char const *in, char const **ret_end)
{
    int n;

    if ((*in & 0x80) == 0) {
        n = *in++ & 0x7F;
    } else if ((*in & 0xE0) == 0xC0) {
        n = (*in++ & 0x1F) << 6;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= *in++ & 0x3F;
        else
            n |= -1;
    } else if ((*in & 0xF0) == 0xE0) {
        n = (*in++ & 0x0F) << 12;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= (*in++ & 0x3F) << 6;
        else
            n |= -1;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= *in++ & 0x3F;
        else
            n |= -1;
    } else if ((*in & 0xF8) == 0xF0) {
        n = (*in++ & 0x07) << 18;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= (*in++ & 0x3F) << 12;
        else
            n |= -1;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= (*in++ & 0x3F) << 6;
        else
            n |= -1;

        if ((*in & 0xC0) != 0x80)
            n |= -1;

        if (*in != 0)
            n |= *in++ & 0x3F;
        else
            n |= -1;
    } else {
        ++in;
        n = -1;
    }

    if (ret_end)
        *ret_end = in;

    return n;
}

// Same semantics as utf8_to_ucs4
EXPORT int utf16_to_ucs4(uint16_t const *in, uint16_t const **ret_end)
{
    if (in[0] < 0xD800 || in[0] > 0xDFFF) {
        if (ret_end)
            *ret_end = in + (*in != 0);
        return *in;
    } else if (in[0] >= 0xD800 && in[0] <= 0xDBFF &&
            in[1] >= 0xDC00 && in[1] <= 0xDFFF) {
        if (ret_end)
            *ret_end = in + 2;
        return ((in[0] - 0xD800) << 10) |
                ((in[1] - 0xDC00) & 0x3FF);
    }
    // Invalid surrogate pair
    return 0;
}

// Same semantics as utf8_to_ucs4
EXPORT int utf16be_to_ucs4(uint16_t const *in, uint16_t const **ret_end)
{
    uint16_t in0 = ntohs(in[0]);

    if (in0 < 0xD800 || in0 > 0xDFFF) {
        if (ret_end)
            *ret_end = in + (in0 != 0);

        return in0;
    } else {
        uint16_t in1 = ntohs(in[1]);

        if (in0 >= 0xD800 && in0 <= 0xDBFF &&
                in1 >= 0xDC00 && in1 <= 0xDFFF) {
            if (ret_end)
                *ret_end = in + 2;

            return ((in0 - 0xD800) << 10) |
                    ((in1 - 0xDC00) & 0x3FF);
        }
    }
    // Invalid surrogate pair
    return 0;
}

void *memfill_16(void *dest, uint16_t v, size_t count)
{
    uint16_t *d = dest;
    for (size_t i = 0; i < count; ++i)
        d[i] = v;
    return dest;
}

void *memfill_32(void *dest, uint32_t v, size_t count)
{
    uint32_t *d = dest;
    for (size_t i = 0; i < count; ++i)
        d[i] = v;
    return dest;
}

void *memfill_64(void *dest, uint64_t v, size_t count)
{
    uint64_t *d = dest;
    for (size_t i = 0; i < count; ++i)
        d[i] = v;
    return dest;
}

size_t utf8_count(char const *in)
{
    size_t count = 0;
    for (char const *p = in; *p; utf8_to_ucs4(p, &p))
        ++count;
    return count;
}

size_t utf16_count(uint16_t const *in)
{
    size_t count;
    for (count = 0; *in; utf16_to_ucs4(in, &in))
        ++count;
    return count;
}
