#include "memcpy.h"
#include "cpuid.h"
#include "likely.h"
#include "assert.h"
#include "string.h"

static void *resolve_memcpy_nt(void *dest, void const *src, size_t n);

typedef void *(*memcpy_fn_t)(void *dest, void const *src, size_t n);

static memcpy_fn_t memcpy_nt_fn = resolve_memcpy_nt;

static void *memcpy_nt_sse4_1(void *dest, void const *src, size_t n)
{
    void *d = dest;
    while (n >= 256) {
        __asm__ __volatile__ (
            "movntdqa      (%[src]),%%xmm0 \n\t"
            "movntdqa  1*16(%[src]),%%xmm1 \n\t"
            "movntdqa  2*16(%[src]),%%xmm2 \n\t"
            "movntdqa  3*16(%[src]),%%xmm3 \n\t"
            "movntdqa  4*16(%[src]),%%xmm4 \n\t"
            "movntdqa  5*16(%[src]),%%xmm5 \n\t"
            "movntdqa  6*16(%[src]),%%xmm6 \n\t"
            "movntdqa  7*16(%[src]),%%xmm7 \n\t"
            "movntdqa  8*16(%[src]),%%xmm8 \n\t"
            "movntdqa  9*16(%[src]),%%xmm9 \n\t"
            "movntdqa 10*16(%[src]),%%xmm10\n\t"
            "movntdqa 11*16(%[src]),%%xmm11\n\t"
            "movntdqa 12*16(%[src]),%%xmm12\n\t"
            "movntdqa 13*16(%[src]),%%xmm13\n\t"
            "movntdqa 14*16(%[src]),%%xmm14\n\t"
            "movntdqa 15*16(%[src]),%%xmm15\n\t"
            "movntdq %%xmm0 ,     (%[dst])\n\t"
            "movntdq %%xmm1 , 1*16(%[dst])\n\t"
            "movntdq %%xmm2 , 2*16(%[dst])\n\t"
            "movntdq %%xmm3 , 3*16(%[dst])\n\t"
            "movntdq %%xmm4 , 4*16(%[dst])\n\t"
            "movntdq %%xmm5 , 5*16(%[dst])\n\t"
            "movntdq %%xmm6 , 6*16(%[dst])\n\t"
            "movntdq %%xmm7 , 7*16(%[dst])\n\t"
            "movntdq %%xmm8 , 8*16(%[dst])\n\t"
            "movntdq %%xmm9 , 9*16(%[dst])\n\t"
            "movntdq %%xmm10,10*16(%[dst])\n\t"
            "movntdq %%xmm11,11*16(%[dst])\n\t"
            "movntdq %%xmm12,12*16(%[dst])\n\t"
            "movntdq %%xmm13,13*16(%[dst])\n\t"
            "movntdq %%xmm14,14*16(%[dst])\n\t"
            "movntdq %%xmm15,15*16(%[dst])\n\t"
            :
            : [src] "r" (src), [dst] "r" (d)
            : "memory"
        );
        src = (__ivec4*)src + 16;
        d = (__ivec4*)d + 16;
        n -= 256;
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
    assert(n == 0);
    __builtin_ia32_sfence();
    return dest;
}

static void *resolve_memcpy_nt(void *dest, void const *src, size_t n)
{
    if (cpuid_has_sse4_1()) {
        memcpy_nt_fn = memcpy_nt_sse4_1;
        return memcpy_nt_sse4_1(dest, src, n);
    }
    memcpy_nt_fn = memcpy;
    return memcpy(dest, src, n);
}

// Non-temporal memcpy.
// Source and destination must be 16-byte aligned
void *memcpy_nt(void *dest, void const *src, size_t n)
{
    return memcpy_nt_fn(dest, src, n);
}
