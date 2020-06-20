#include <string.h>
#include <stdint.h>
#include <sys/likely.h>
#include <limits.h>

#if defined(__x86_64__) || defined(__i386__)
using v32byte = unsigned char __attribute__((__vector_size__(32)));
using fast_t = v32byte;

static inline fast_t memset_broadcast(int c)
{
    unsigned char b = c;
    return fast_t{b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b,
        b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b};
}
#else
using fast_t = int64_t;

static inline fast_t memset_broadcast(int c)
{
    c &= 0xFF;
    return c * 0x0101010101010101;
}
#endif

static inline bool is_fastpath(void *lhs, size_t sz)
{
    return (uintptr_t(lhs) & -sizeof(fast_t)) == uintptr_t(lhs) &&
            (sz & -sizeof(fast_t)) == sz;
}

static inline void *memset_fastpath(void *lhs, int c, size_t sz)
{
    fast_t value = memset_broadcast(c);
    fast_t *dest = reinterpret_cast<fast_t*>(lhs);
    sz /= sizeof(fast_t);
    while (sz--)
        *dest++ = value;
    return lhs;
}

void *memset(void *lhs, int c, size_t sz)
{
    if (likely(is_fastpath(lhs, sz)))
        return memset_fastpath(lhs, c, sz);

    auto d = (char *)lhs;
    auto cc = char(c);

    for (size_t i = 0; i < sz; ++i)
        d[i] = cc;

    return d;
}
