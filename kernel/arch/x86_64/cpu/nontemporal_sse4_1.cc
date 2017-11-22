#include "types.h"
#include "likely.h"
#include "intrin_compat.h"

extern "C" void *memcpy512_nt_sse4_1(void *dest, void const *src, size_t n)
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
        src = (__i32_vec4*)src + 4;
        d = (__i32_vec4*)d + 4;
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
        src = (__i32_vec4*)src + 1;
        d = (__i32_vec4*)d + 1;
        n -= 16;
    }
    return dest;
}

extern "C" void *memcpy32_nt_sse4_1(void *dest, void const *src, size_t n)
{
    int *dd = (int*)dest;
    int const *ds = (int const *)src;

    if (unlikely(uintptr_t(dest) & 7))
        __builtin_ia32_movnti(dd++,  *ds++);

    long long *qd = (long long*)dd;
    long long *qs = (long long*)ds;

    while (n >= 64) {
        __builtin_ia32_movnti64(&qd[0],  qs[0]);
        __builtin_ia32_movnti64(&qd[1],  qs[1]);
        __builtin_ia32_movnti64(&qd[2],  qs[2]);
        __builtin_ia32_movnti64(&qd[3],  qs[3]);
        __builtin_ia32_movnti64(&qd[4],  qs[4]);
        __builtin_ia32_movnti64(&qd[5],  qs[5]);
        __builtin_ia32_movnti64(&qd[6],  qs[6]);
        __builtin_ia32_movnti64(&qd[7],  qs[7]);
        qd += 8;
        qs += 8;
        n -= 64;
    }

    dd = (int*)qd;
    ds = (int*)qs;

    while (n >= 4) {
        __builtin_ia32_movnti(dd++, *ds++);
        n -= 4;
    }
    return dest;
}

extern "C" void *memset32_nt_sse4_1(void *dest, uint32_t val, size_t n)
{
    void *d = dest;

    // Write 32-bit values until it is 128-bit aligned
    while (((uintptr_t)d & 0xC) && n >= 4) {
        __builtin_ia32_movnti((int*)d, val);
        d = (uint32_t*)d + 1;
        n -= 4;
    }

    // Write as many 128-bit values as possible
    __u32_vec4 val128 = { val, val, val, val };
    while (n >= 16) {
        __builtin_ia32_movntdq((__i64_vec2LL*)d, (__i64_vec2LL)val128);
        d = (__i32_vec4*)d + 1;
        n -= 16;
    }

    // Write remainder
    while (n >= 4) {
        __builtin_ia32_movnti((int*)d, val);
        d = (uint32_t*)d + 1;
        n -= 4;
    }

    return dest;
}
