#include <stdint.h>
#include <sys/cdefs.h>

extern "C" void *(*memset_resolver())(void *, int, size_t);
extern "C" void *memset(void *, int, size_t)
    __attribute__((__ifunc__("memset_resolver")));

#include <string.h>
#include <sys/likely.h>
#include <limits.h>

using v32byte = unsigned char __attribute__((__vector_size__(32)));
using v16byte = unsigned char __attribute__((__vector_size__(16)));

#ifdef __DGOS_KERNEL__
#define HAVE_256 0
#define HAVE_128 0
#define HAVE_INT 1
#elif defined(__x86_64__) || defined(__i386__)
#define TARGET_256 __attribute__((__target__("avx"), \
                                  __optimize__("tree-vectorize")))
#define TARGET_128 __attribute__((__target__("sse2"), \
                                  __optimize__("tree-vectorize")))
#define TARGET_INT __attribute__((__target__("no-sse")))
#define HAVE_256 1
#define HAVE_128 1
#define HAVE_INT 0
#else
#error what then
#endif

TARGET_256
static constexpr v32byte memset_broadcast_256(int c)
{
    unsigned char b = c;
    
    return v32byte{
        b, b, b, b, b, b, b, b,
        b, b, b, b, b, b, b, b,
        b, b, b, b, b, b, b, b,
        b, b, b, b, b, b, b, b
    };
}

TARGET_128
static constexpr v16byte memset_broadcast_128(int c)
{
    unsigned char b = c;
    
    return v16byte{
        b, b, b, b, b, b, b, b,
        b, b, b, b, b, b, b, b
    };
}

TARGET_INT
static constexpr inline intmax_t memset_broadcast_int(int c)
{
    return intmax_t(c & 0xFF) * 0x0101010101010101;
}

static bool is_fastpath_int(void *lhs, size_t sz)
{
    return (uintptr_t(lhs) & -sizeof(intmax_t)) == uintptr_t(lhs) &&
            (sz & -sizeof(intmax_t)) == sz;
}

static bool is_fastpath_128(void *lhs, size_t sz)
{
    return (uintptr_t(lhs) & -sizeof(v16byte)) == uintptr_t(lhs) &&
            (sz & -sizeof(v16byte)) == sz;
}

static bool is_fastpath_256(void *lhs, size_t sz)
{
    return (uintptr_t(lhs) & -sizeof(v32byte)) == uintptr_t(lhs) &&
            (sz & -sizeof(v32byte)) == sz;
}

TARGET_INT
static void *memset_fastpath_int(void *lhs, int c, size_t sz)
{
    if (uintptr_t(lhs) & ~-sizeof(intmax_t))
        __builtin_unreachable();
    if (sz & ~-sizeof(intmax_t))
        __builtin_unreachable();
    
    intmax_t value = memset_broadcast_int(c);
    intmax_t *dest = reinterpret_cast<intmax_t*>(lhs);
    sz /= sizeof(*dest);
    while (sz--)
        *dest++ = value;
    return lhs;
}

TARGET_128
static void *memset_fastpath_128(void *lhs, int c, size_t sz)
{
    if (uintptr_t(lhs) & ~-sizeof(v16byte))
        __builtin_unreachable();
    if (sz & ~-sizeof(v16byte))
        __builtin_unreachable();
    
    v16byte value = memset_broadcast_128(c);
    v16byte *dest = reinterpret_cast<v16byte*>(lhs);
    sz /= sizeof(*dest);
    while (sz--)
        *dest++ = value;
    return lhs;
}

TARGET_256
static inline void *memset_fastpath_256(void *lhs, int c, size_t sz)
{
    if (uintptr_t(lhs) & ~-sizeof(v32byte))
        __builtin_unreachable();
    if (sz & ~-sizeof(v32byte))
        __builtin_unreachable();
    
    v32byte value = memset_broadcast_256(c);
    v32byte *dest = reinterpret_cast<v32byte*>(lhs);
    sz /= sizeof(*dest);
    while (sz--)
        *dest++ = value;
    return lhs;
}

static void *memset_slow(char *d, char cc, size_t sz)
{
    for (size_t i = 0; i < sz; ++i)
        d[i] = cc;
    
    return d;
}

void *memset_int(void *lhs, int c, size_t sz)
{
    if (likely(is_fastpath_int(lhs, sz)))
        return memset_fastpath_int(lhs, c, sz);
    
    // Slowpath
    return memset_slow((char*)lhs, (char)c, sz);
}

void *memset_128(void *lhs, int c, size_t sz)
{
    if (likely(is_fastpath_128(lhs, sz)))
        return memset_fastpath_128(lhs, c, sz);

    // Slowpath
    return memset_slow((char*)lhs, (char)c, sz);
}

void *memset_256(void *lhs, int c, size_t sz)
{
    if (likely(is_fastpath_256(lhs, sz)))
        return memset_fastpath_256(lhs, c, sz);
    
    // Slowpath
    return memset_slow((char*)lhs, (char)c, sz);
}

void *(*memset_resolver())(void *, int, size_t)
{
    if (__builtin_cpu_supports("avx2"))
        return memset_256;
    if (__builtin_cpu_supports("sse2"))
        return memset_128;
    return memset_int;
}
