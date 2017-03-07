#include "nontemporal.h"
#include "cpuid.h"
#include "likely.h"
#include "assert.h"
#include "string.h"

static void *resolve_memcpy512_nt(void *dest, void const *src, size_t n);
static void *resolve_memcpy32_nt(void *dest, void const *src, size_t n);
static void *resolve_memset32_nt(void *dest, uint32_t val, size_t n);

typedef void *(*memcpy_fn_t)(void *dest, void const *src, size_t n);
typedef void *(*memset32_fn_t)(void *dest, uint32_t val, size_t n);

static memcpy_fn_t memcpy512_nt_fn = resolve_memcpy512_nt;
static memcpy_fn_t memcpy32_nt_fn = resolve_memcpy32_nt;
static memset32_fn_t memset32_nt_fn = resolve_memset32_nt;

static void *memcpy512_nt_sse4_1(void *dest, void const *src, size_t n)
{
    void *d = dest;
    while (n >= 64) {
        __asm__ __volatile__ (
            "movntdqa      (%[src]),%%xmm0 \n\t"
            "movntdqa  1*16(%[src]),%%xmm1 \n\t"
            "movntdqa  2*16(%[src]),%%xmm2 \n\t"
            "movntdqa  3*16(%[src]),%%xmm3 \n\t"
            "movntdq %%xmm0 ,     (%[dst])\n\t"
            "movntdq %%xmm1 , 1*16(%[dst])\n\t"
            "movntdq %%xmm2 , 2*16(%[dst])\n\t"
            "movntdq %%xmm3 , 3*16(%[dst])\n\t"
            :
            : [src] "r" (src), [dst] "r" (d)
            : "memory"
        );
        src = (__ivec4*)src + 4;
        d = (__ivec4*)d + 4;
        n -= 64;
    }
    while (unlikely(n >= 16)) {
        __asm__ __volatile__ (
            "movntdqa      (%[src]),%%xmm0 \n\t"
            "movntdq %%xmm0 ,     (%[dst])\n\t"
            :
            : [src] "r" (src), [dst] "r" (d)
            : "memory"
        );
        src = (__ivec4*)src + 1;
        d = (__ivec4*)d + 1;
        n -= 16;
    }
    return dest;
}

static void *resolve_memcpy512_nt(void *dest, void const *src, size_t n)
{
    if (cpuid_has_sse4_1()) {
        memcpy512_nt_fn = memcpy512_nt_sse4_1;
        return memcpy512_nt_sse4_1(dest, src, n);
    }
    memcpy512_nt_fn = memcpy;
    return memcpy(dest, src, n);
}

// Non-temporal memcpy.
// Source and destination must be 64-byte aligned
void *memcpy512_nt(void *dest, void const *src, size_t n)
{
    assert(!((uintptr_t)dest & 63));
    assert(!((uintptr_t)src & 63));
    assert(!((uintptr_t)n & 63));
    return memcpy512_nt_fn(dest, src, n);
}

static void *memcpy32_nt_sse4_1(void *dest, void const *src, size_t n)
{
    int *d = dest;
    int const *s = src;
    while (n >= 4) {
        __builtin_ia32_movnti(d++, *s++);
        n -= 4;
    }
    return dest;
}

static void *resolve_memcpy32_nt(void *dest, void const *src, size_t n)
{
    if (cpuid_has_sse4_1()) {
        memcpy32_nt_fn = memcpy32_nt_sse4_1;
        return memcpy32_nt_sse4_1(dest, src, n);
    }
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
    __builtin_ia32_sfence();
}

//
// Non-temporal memset

static void *memset32_nt_sse(void *dest, uint32_t val, size_t n)
{
    void *d = dest;

    while (n >= 4) {
        __builtin_ia32_movnti(d, val);
        d = (uint32_t*)d + 1;
        n -= 4;
    }

    return dest;
}

static void *memset32_nt_sse4_1(void *dest, uint32_t val, size_t n)
{
    void *d = dest;

    // Write 32-bit values until it is 128-bit aligned
    while (((uintptr_t)d & 0xC) && n >= 4) {
        __builtin_ia32_movnti(d, val);
        d = (uint32_t*)d + 1;
        n -= 4;
    }

    // Write as many 128-bit values as possible
    __ivec4 val128 = { val, val, val, val };
    while (n >= 16) {
        __builtin_ia32_movntdq(d, (__ivec2LL)val128);
        d = (__ivec4*)d + 1;
        n -= 16;
    }

    // Write remainder
    while (n >= 4) {
        __builtin_ia32_movnti(d, val);
        d = (uint32_t*)d + 1;
        n -= 4;
    }

    return dest;
}

static void *resolve_memset32_nt(void *dest, uint32_t val, size_t n)
{
    if (cpuid_has_sse4_1()) {
        memset32_nt_fn = memset32_nt_sse4_1;
        return memset32_nt_sse4_1(dest, val, n);
    }
    memset32_nt_fn = memset32_nt_sse;
    return memset32_nt_sse(dest, val, n);
}

void memset32_nt(void *dest, uint32_t val, size_t n)
{
    assert(!((uintptr_t)dest & 3));
    assert(!((uintptr_t)n & 3));
    memset32_nt_fn(dest, val, n);
}
