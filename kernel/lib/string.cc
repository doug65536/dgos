#define __NO_STRING_BUILTIN
#include "string.h"
#undef __NO_STRING_BUILTIN
#include "export.h"
#include "bswap.h"
#include "assert.h"
#include "cpu/nontemporal.h"

#define USE_REP_STRING 1

EXPORT size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

EXPORT void *memchr(void const *mem, int ch, size_t count)
{
    for (char const *p = (char const *)mem; count--; ++p)
        if (*p == (char)ch)
            return (void *)p;
   return 0;
}

EXPORT void *memrchr(void const *mem, int ch, size_t count)
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
            return (char*)s;
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
    int cmp = 0;
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
#ifdef USE_REP_STRING
    int result = 0;
    register int below asm("r8") = -1;
    __asm__ __volatile__ (
        "xor %[result],%[result]\n\t"
        "repe cmpsb\n\t"
        "cmovb %[below],%[result]\n\t"
        "cmova %[above],%[result]\n\t"
        : [result] "=a" (result)
        , "+S" (lhs)
        , "+D" (rhs)
        , "+c" (count)
        : [below] "r" (below)
        , [above] "d" (1)
    );
    return result;
#else
    unsigned char const *lp = (unsigned char const *)lhs;
    unsigned char const *rp = (unsigned char const *)rhs;
    int cmp = 0;
    if (count) {
        do {
            cmp = *lp++ - *rp++;
        } while (cmp == 0 && --count);
    }
    return cmp;
#endif
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

static void memset16(char *&d, uint64_t s, size_t n)
{
    __asm__ __volatile__ (
        "rep stosw"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

static void memset32(char *&d, uint64_t s, size_t n)
{
    __asm__ __volatile__ (
        "rep stosl"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

static void memset64(char *&d, uint64_t s, size_t n)
{
    __asm__ __volatile__ (
        "rep stosq"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

static void memset_byte(char *&d, uint64_t s, uint32_t &ofs)
{
    __asm__ __volatile__ (
        "mov %%al,(%%rdi,%%rdx)\n\t"
        "add $1,%%rdx\n\t"
        : "+D" (d)
        , "+d" (ofs)
        : "a" (s)
        : "memory"
    );
}

EXPORT void *memset(void *dest, int c, size_t n)
{
    char *d = (char*)dest;
    uint64_t s = size_t(0x0101010101010101U) * (c & 0xFFU);

    if (likely(n >= 7) && unlikely(uintptr_t(d) & 7)) {
        uint32_t ofs = 0;
        switch (uintptr_t(d) & 7) {
        case 1: memset_byte(d, s, ofs); // fall thru
        case 2: memset_byte(d, s, ofs); // fall thru
        case 3: memset_byte(d, s, ofs); // fall thru
        case 4: memset_byte(d, s, ofs); // fall thru
        case 5: memset_byte(d, s, ofs); // fall thru
        case 6: memset_byte(d, s, ofs); // fall thru
        case 7: memset_byte(d, s, ofs); // fall thru
        }

        n -= ofs;
        if (unlikely(!n))
            return dest;

        d += ofs;
    }

    size_t quads = n >> 3;
    memset64(d, s, quads);

    if (unlikely(n &= 7)) {
        uint32_t ofs = 0;
        switch (n) {
        case 7: memset_byte(d, s, ofs); // fall thru
        case 6: memset_byte(d, s, ofs); // fall thru
        case 5: memset_byte(d, s, ofs); // fall thru
        case 4: memset_byte(d, s, ofs); // fall thru
        case 3: memset_byte(d, s, ofs); // fall thru
        case 2: memset_byte(d, s, ofs); // fall thru
        case 1: memset_byte(d, s, ofs); // fall thru
        case 0: break;
        }
    }

    return dest;
}

// GCC erroneously disables __builtin_ia32_movnti64
// when using mgeneral-regs-only
__attribute__((target("sse2")))
void clear64(void *dest, size_t n)
{
    long long *d = (long long *)dest;

    do {
        __builtin_ia32_movnti64(d, 0);
        __builtin_ia32_movnti64(d+1, 0);
        __builtin_ia32_movnti64(d+2, 0);
        __builtin_ia32_movnti64(d+3, 0);
        __builtin_ia32_movnti64(d+4, 0);
        __builtin_ia32_movnti64(d+5, 0);
        __builtin_ia32_movnti64(d+6, 0);
        __builtin_ia32_movnti64(d+7, 0);
        d += 8;
    } while (n -= 64);
    __builtin_ia32_sfence();
}

static __always_inline void memcpy_byte(char *d, char const *s, uint32_t &ofs)
{
    uint32_t eax;
    __asm__ __volatile__(
        "movzbl (%%rsi,%%rdx),%%eax\n\t"
        "movb %%al,(%%rdi,%%rdx)\n\t"
        "addl $1,%%edx\n\t"
        : "+d" (ofs)
        , "=a" (eax)
        : "D" (d)
        , "S" (s)
        : "memory"
    );
}

static __always_inline void memcpy64(char *&d, char const *&s, size_t count)
{
    __asm__ __volatile__ (
        "rep movsq\n\t"
        : "+D" (d)
        , "+S" (s)
        , "+c" (count)
        :
        : "memory"
    );
}

EXPORT void *memcpy(void *dest, void const *src, size_t n)
{
#ifdef USE_REP_STRING
    char *d = (char*)dest;
    char const *s = (char const *)src;

    if (n >= 7) {
        if (unlikely(uintptr_t(d) & 7)) {
            uint32_t ofs = 0;
            switch (uintptr_t(d) & 7) {
            case 1: memcpy_byte(d, s, ofs); // fall thru
            case 2: memcpy_byte(d, s, ofs); // fall thru
            case 3: memcpy_byte(d, s, ofs); // fall thru
            case 4: memcpy_byte(d, s, ofs); // fall thru
            case 5: memcpy_byte(d, s, ofs); // fall thru
            case 6: memcpy_byte(d, s, ofs); // fall thru
            case 7: memcpy_byte(d, s, ofs); // fall thru
            }

            n -= ofs;
            if (unlikely(!n))
                return dest;

            d += ofs;
            s += ofs;
        }
    }

    size_t quads = n >> 3;
    memcpy64(d, s, quads);

    if (unlikely(n &= 7)) {
        uint32_t ofs = 0;
        switch (n) {
        case 7: memcpy_byte(d, s, ofs); // fall thru
        case 6: memcpy_byte(d, s, ofs); // fall thru
        case 5: memcpy_byte(d, s, ofs); // fall thru
        case 4: memcpy_byte(d, s, ofs); // fall thru
        case 3: memcpy_byte(d, s, ofs); // fall thru
        case 2: memcpy_byte(d, s, ofs); // fall thru
        case 1: memcpy_byte(d, s, ofs); // fall thru
        }
    }
#else
    char *d;
    char const *s;

#ifdef __GNUC__
    // If source and destination are aligned, copy 128 bits at a time
    if (n >= 16 && !((intptr_t)dest & 0x0F) && !((intptr_t)src & 0x0F)) {
        __i64_vec2 *vd = dest;
        __i64_vec2 const *vs = src;
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

static __always_inline void memcpy_byte_reverse(
        char *d, char const *s, size_t &count)
{
    uint32_t eax;
    __asm__ __volatile__ (
        "movzbl (%%rsi,%%rdx),%%eax\n"
        "movb %%al,(%%rdi,%%rdx)\n\t"
        "addq $-1,%%rdx\n\t"
        : "+d" (count)
        , "=a" (eax)
        : "D" (d)
        , "S" (s)
        : "memory"
    );
}

static __always_inline void memcpy64_reverse(
        char *&d, char const *&s, size_t count)
{
    __asm__ __volatile__ (
        "std\n\t"
        "rep movsq\n\t"
        "cld\n\t"
        : "+D" (d)
        , "+S" (s)
        , "+c" (count)
        :
        : "memory"
    );
}

static __always_inline void memcpy_reverse(
        void *dest, void const *src, size_t n)
{
#if USE_REP_STRING
    char *d = (char *)dest;
    char const *s = (char const *)src;


    if (uintptr_t(d) & 7) {
        size_t ofs = 0;
        switch (uintptr_t(d) & 7) {
        case 7: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 6: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 5: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 4: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 3: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 2: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 1: memcpy_byte_reverse(d, s, ofs); // fall thru
        }

        // plus because ofs is negative
        n += ofs;
        if (unlikely(!n))
            return;

        d += ofs;
        s += ofs;
    }

    size_t quads = n >> 3;
    memcpy64_reverse(d, s, quads);

    if (n &= 7) {
        size_t ofs = 0;
        switch (n) {
        case 7: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 6: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 5: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 4: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 3: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 2: memcpy_byte_reverse(d, s, ofs); // fall thru
        case 1: memcpy_byte_reverse(d, s, ofs); // fall thru
        }
    }
#else
    for (size_t i = n; i; --i)
        dest[i-1] = src[i-1];
#endif
}

EXPORT void *memmove(void *dest, void const *src, size_t n)
{
    char *d = (char *)dest;
    char const *s = (char const *)src;

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

EXPORT char *stpcpy(char *lhs, const char *rhs)
{
    auto d = (char *)lhs;
    auto s = (char const *)rhs;
    size_t i;
    for (i = 0; (d[i] = s[i]) != 0; ++i);
    return d + i;
}

static void makeByteBitmap(uint32_t *map, char const *chars)
{
    memset(map, 0, sizeof(*map) * (256 / sizeof(*map)));

    while (*(uint8_t*)chars) {
        size_t ch = *(uint8_t*)chars++;
        map[ch >> 5] |= (1U << (ch & 31));
    }
}

EXPORT size_t strspn(char const *src, char const *chars)
{
    size_t i = 0;

    if (chars[0] && !chars[1]) {
        // One character special case
        while (src[i] && src[i] == chars[0])
            ++i;
    } else {
        // Generalize
        uint32_t map[256 / sizeof(uint32_t)];

        for (makeByteBitmap(map, chars); src[i]; ++i) {
            size_t ch = (uint8_t)src[i];
            if (!(map[ch >> 5] & (1U << (ch & 31))))
                break;
        }
    }

    return i;
}

EXPORT size_t strcspn(char const *src, char const *chars)
{
    size_t i = 0;

    if (chars[0] && !chars[1]) {
        // One character special case
        while (src[i] && src[i] != chars[0])
            ++i;
    } else {
        // Generalize
        uint32_t map[256 / sizeof(uint32_t)];

        for (makeByteBitmap(map, chars); src[i]; ++i) {
            size_t ch = (uint8_t)src[i];
            if (map[ch >> 5] & (1U << (ch & 31)))
                break;
        }
    }

    return i;
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

void memfill_16(void *dest, uint16_t v, size_t count)
{
    char *d = (char*)dest;
    memset16(d, v, count);
}

void memfill_32(void *dest, uint32_t v, size_t count)
{
    char *d = (char*)dest;
    memset32(d, v, count);
}

void memfill_64(void *dest, uint64_t v, size_t count)
{
    char *d = (char*)dest;
    memset64(d, v, count);
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
