#define __NO_STRING_BUILTIN
#include "string.h"
#undef __NO_STRING_BUILTIN
#include "export.h"
#include "bswap.h"
#include "assert.h"
#include "cpu/nontemporal.h"

#define USE_REP_STRING 1

#ifdef USE_REP_STRING
EXPORT void memcpy_movsq(void *dest, void *src, size_t u64_count)
{
    __asm__ __volatile__ (
        "rep movsq"
        : "+D" (dest)
        , "+S" (src)
        , "+c" (u64_count)
        :
        : "memory"
    );
}

// Parameter order puts parameters in the right registers
EXPORT void memset_stosq(void *dest, uint64_t value, size_t u64_count)
{
    value &= 0xFF;
    value *= UINT64_C(0x0101010101010101);
    __asm__ __volatile__ (
        "rep stosq"
        : "+D" (dest)
        , "+c" (u64_count)
        : "a" (value)
        : "memory"
    );
}
#endif

EXPORT size_t strlen(char const *src)
{
    size_t len = 0;
    for ( ; src[len]; ++len);
    return len;
}

EXPORT void *memchr(void const *mem, int ch, size_t count)
{
    unsigned char c = (unsigned char)ch;
    unsigned char const *p;
    for (p = (unsigned char const *)mem; count--; ++p)
        if (*p == c)
            return (void *)p;
    return nullptr;
}

EXPORT void *memrchr(void const *mem, int ch, size_t count)
{
    unsigned char c = (unsigned char)ch;
    unsigned char const *p;
    for (p = (unsigned char const *)mem + count; count--; --p)
        if (p[-1] == c)
            return (void*)(p - 1);
    return nullptr;
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
EXPORT char *strchr(char const *s, int ch)
{
    unsigned char v, c = (unsigned char)ch;
    for (size_t i = 0; ; ++i) {
        v = (unsigned char)s[i];
        if (v == c)
            return (char*)s + i;
        if (v == 0)
            return nullptr;
    }
}

// The terminating null character is considered to be a part
// of the string and can be found when searching for '\0'.
EXPORT char *strrchr(char const *s, int ch)
{
    unsigned char v, c = (unsigned char)ch;

    // One pass algorithm finds last occurrence of ch and length
    size_t best = 0;
    for (size_t i = 0; ; ++i) {
        v = (unsigned char)s[i];

        if (v == c)
            best = i + 1;

        if (v == 0)
            break;
    }

    return best ? (char *)(s + best - 1) : nullptr;
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

// Security enhanced memcmp which prevents inferring how many characters match
EXPORT int const_time_memcmp(void const *lhs, void const *rhs, size_t count)
{
    uint8_t const *blhs = (uint8_t const *)lhs;
    uint8_t const *brhs = (uint8_t const *)rhs;
    int result = 0;
    for (size_t i = 0; i < count; ++i) {
        int diff = *brhs - *blhs;
        result = result ? result : diff;
    }
    return result;
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
        return nullptr;

    // Only search as far as substr would fit within str
    size_t chklen = slen - blen;

    // Try each starting point
    for (size_t i = 0; i <= chklen; ++i)
        if (memcmp(str + i, substr, blen) == 0)
            return (char*)(str + i);

    return nullptr;
}

static _always_inline void memset_byte(char *&d, uint64_t s, ptrdiff_t &ofs)
{
    __asm__ __volatile__ (
        "movb %b[s],(%[d],%[ofs])\n\t"
        "add $ 1,%%rdx\n\t"
        : [d] "+D" (d)
        , [ofs] "+d" (ofs)
        : [s] "a" (s)
        : "memory"
    );
}

EXPORT void *memset(void *dest, int c, size_t n)
{
    char *d = (char*)dest;
    uint64_t s = 0x0101010101010101U * (c & 0xFFU);

    if (likely(n >= 7) && unlikely(uintptr_t(d) & 7)) {
        ptrdiff_t ofs = 0;
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
        ptrdiff_t ofs = 0;
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

#if USE_REP_STRING
void clear64(void *dest, size_t n)
{
    memset_stosq(dest, 0, n);
}
#else
// GCC erroneously disables __builtin_ia32_movnti64
// when using mgeneral-regs-only
__attribute__((target("sse2")))
void clear64(void *dest, size_t u64_count)
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
    } while (u64_count--);
    __builtin_ia32_sfence();
}
#endif

static _always_inline void memcpy_byte(char *d, char const *s, uint32_t &ofs)
{
    uint32_t eax;
    __asm__ __volatile__(
        "movzbl (%%rsi,%%rdx),%%eax\n\t"
        "movb %%al,(%%rdi,%%rdx)\n\t"
        "addl $ 1,%%edx\n\t"
        : "+d" (ofs)
        , "=a" (eax)
        : "D" (d)
        , "S" (s)
        : "memory"
    );
}

static _always_inline void memcpy64(char *&d, char const *&s, size_t count)
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

static _always_inline void memcpy_byte_reverse(
        char *d, char const *s, size_t &count)
{
    uint32_t eax;
    __asm__ __volatile__ (
        "movzbl (%%rsi,%%rdx),%%eax\n"
        "movb %%al,(%%rdi,%%rdx)\n\t"
        "addq $ -1,%%rdx\n\t"
        : "+d" (count)
        , "=a" (eax)
        : "D" (d)
        , "S" (s)
        : "memory"
    );
}

static _always_inline void memcpy64_reverse(
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

static _always_inline void memcpy_reverse(
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

EXPORT char *stpcpy(char *lhs, char const *rhs)
{
    auto d = lhs;
    auto s = rhs;
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
        unsigned map[256 / sizeof(unsigned)];

        for (makeByteBitmap(map, chars); src[i]; ++i) {
            size_t ch = (uint8_t)src[i];
            if (!(map[ch >> 5] & (1U << (ch & 31))))
                break;
        }
    }

    return i;
}

// Return the offset of a character in the chars string
// otherwise, the length of the string
EXPORT size_t strcspn(char const *src, char const *chars)
{
    size_t i = 0;

    if (chars[0] && !chars[1]) {
        // One character special case, fastpath
        while (src[i] && src[i] != chars[0])
            ++i;
    } else {
        // Generalize
        unsigned map[256 / sizeof(unsigned)];

        for (makeByteBitmap(map, chars); src[i]; ++i) {
            size_t ch = (uint8_t)src[i];
            if (map[ch >> 5] & (size_t(1) << (ch & 31)))
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
