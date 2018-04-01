#include "nontemporal.h"
#include "assert.h"
#include "cpuid.h"
#include "likely.h"
#include "string.h"
#include "printk.h"

#define DEBUG_NONTEMPORAL   1
#if DEBUG_NONTEMPORAL
#define NONTEMPORAL_TRACE(...) printdbg("nontemporal: " __VA_ARGS__)
#else
#define NONTEMPORAL_TRACE(...) ((void)0)
#endif

static void *resolve_memcpy512_nt(void *dest, void const *src, size_t n);
static void *resolve_memcpy32_nt(void *dest, void const *src, size_t n);
static void *resolve_memset32_nt(void *dest, uint32_t val, size_t n);

typedef void *(*memcpy_fn_t)(void *dest, void const *src, size_t n);
typedef void *(*memset32_fn_t)(void *dest, uint32_t val, size_t n);

static memcpy_fn_t memcpy512_nt_fn = resolve_memcpy512_nt;
static memcpy_fn_t memcpy32_nt_fn = resolve_memcpy32_nt;
static memset32_fn_t memset32_nt_fn = resolve_memset32_nt;

#ifndef __DGOS_KERNEL__
// In separate file due to extra compiler option needed
extern "C" void *memcpy512_nt_avx512(void *dest, void const *src, size_t n);
extern "C" void *memcpy512_nt_avx(void *dest, void const *src, size_t n);
extern "C" void *memcpy512_nt_sse4_1(void *dest, void const *src, size_t n);

extern "C" void *memcpy32_nt_sse4_1(void *dest, void const *src, size_t n);
extern "C" void *memset32_nt_sse4_1(void *dest, uint32_t val, size_t n);
extern "C" void *memcpy32_nt_avx(void *dest, void const *src, size_t n);

extern "C" void *memset32_nt_avx(void *dest, uint32_t val, size_t n);
#endif

static void *resolve_memcpy512_nt(void *dest, void const *src, size_t n)
{
#ifndef __DGOS_KERNEL__
    if (cpuid_has_avx512f()) {
        NONTEMPORAL_TRACE("using avx512 memcpy512\n");
        memcpy512_nt_fn = memcpy512_nt_avx512;
        return memcpy512_nt_avx512(dest, src, n);
    } else if (cpuid_has_avx()) {
        NONTEMPORAL_TRACE("using avx memcpy512\n");
        memcpy512_nt_fn = memcpy512_nt_avx;
        return memcpy512_nt_avx(dest, src, n);
    } else if (cpuid_has_sse4_1()) {
        NONTEMPORAL_TRACE("using sse4.1 memcpy512\n");
        memcpy512_nt_fn = memcpy512_nt_sse4_1;
        return memcpy512_nt_sse4_1(dest, src, n);
    }
#endif
    NONTEMPORAL_TRACE("using legacy memcpy512\n");
    memcpy512_nt_fn = memcpy;
    return memcpy(dest, src, n);
}

// Non-temporal memcpy.
// Source and destination must be 64-byte aligned
void *memcpy512_nt(void *dest, void const *src, size_t n)
{
    assert(!((uintptr_t)dest & 63));
    assert(!((uintptr_t)src & 63));
    assert(!(n & 63));
    return memcpy512_nt_fn(dest, src, n);
}

static void *resolve_memcpy32_nt(void *dest, void const *src, size_t n)
{
#ifndef __DGOS_KERNEL__
    if (cpuid_has_avx()) {
        NONTEMPORAL_TRACE("using avx memcpy32\n");
        memcpy32_nt_fn = memcpy32_nt_avx;
        return memcpy32_nt_avx(dest, src, n);
    } else if (cpuid_has_sse4_1()) {
        NONTEMPORAL_TRACE("using sse4.1 memcpy32\n");
        memcpy32_nt_fn = memcpy32_nt_sse4_1;
        return memcpy32_nt_sse4_1(dest, src, n);
    }
#endif
    NONTEMPORAL_TRACE("using legacy memcpy32\n");
    memcpy32_nt_fn = memcpy;
    return memcpy(dest, src, n);
}

// Non-temporal memcpy.
// Source and destination must be 16-byte aligned
void *memcpy32_nt(void *dest, void const *src, size_t n)
{
    return memcpy32_nt_fn(dest, src, n);
}

void memcpy_nt_fence(void)
{
#ifndef __DGOS_KERNEL__
    __builtin_ia32_sfence();
#endif
}

//
// Non-temporal memset

static void *memset32_nt_sse(void *dest, uint32_t val, size_t n)
{
    int32_t *d = (int32_t*)dest;

    while (n >= 4) {
#ifndef __DGOS_KERNEL__
        __builtin_ia32_movnti(d++, val);
#else
        *d++ = val;
#endif
        n -= 4;
    }

    return dest;
}

static void *resolve_memset32_nt(void *dest, uint32_t val, size_t n)
{
#ifndef __DGOS_KERNEL__
    if (cpuid_has_avx()) {
        NONTEMPORAL_TRACE("using avx memset\n");
        memset32_nt_fn = memset32_nt_avx;
        return memset32_nt_avx(dest, val, n);
    } else if (cpuid_has_sse4_1()) {
        NONTEMPORAL_TRACE("using sse4.1 memset\n");
        memset32_nt_fn = memset32_nt_sse4_1;
        return memset32_nt_sse4_1(dest, val, n);
    }
#endif
    NONTEMPORAL_TRACE("using sse2 memset\n");
    memset32_nt_fn = memset32_nt_sse;
    return memset32_nt_sse(dest, val, n);
}

void memset32_nt(void *dest, uint32_t val, size_t n)
{
    assert(!((uintptr_t)dest & 3));
    assert(!(n & 3));
    memset32_nt_fn(dest, val, n);
}
