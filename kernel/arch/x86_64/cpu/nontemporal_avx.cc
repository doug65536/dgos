#include "types.h"
#include "likely.h"
#include "intrin_compat.h"

extern "C" void *memcpy512_nt_avx(void *dest, void const *src, size_t n)
{
    void *d = dest;
    while (n >= 128) {
        __asm__ __volatile__ (
            "vmovntdqa      (%[src]),%%ymm0 \n\t"
            "vmovntdqa  2*16(%[src]),%%ymm1 \n\t"
            "vmovntdqa  4*16(%[src]),%%ymm2 \n\t"
            "vmovntdqa  6*16(%[src]),%%ymm3 \n\t"
            "vmovntdq %%ymm0 ,     (%[dst])\n\t"
            "vmovntdq %%ymm1 , 2*16(%[dst])\n\t"
            "vmovntdq %%ymm2 , 4*16(%[dst])\n\t"
            "vmovntdq %%ymm3 , 6*16(%[dst])\n\t"
            :
            : [src] "r" (src), [dst] "r" (d)
            : "memory", "%ymm0", "%ymm1", "%ymm2", "%ymm3"
        );
        src = (__ivec4*)src + 8;
        d = (__ivec4*)d + 8;
        n -= 128;
    }
    __asm__ __volatile__ (
        "vzeroupper\n\t"
        :
        :
        : "%ymm0",  "%ymm1",  "%ymm2",  "%ymm3"
        , "%ymm4",  "%ymm5",  "%ymm6",  "%ymm7"
        , "%ymm8",  "%ymm9",  "%ymm10", "%ymm11"
        , "%ymm12", "%ymm13", "%ymm14", "%ymm15"
    );
    while (unlikely(n >= 16)) {
        __asm__ __volatile__ (
            "vmovntdqa      (%[src]),%%xmm0 \n\t"
            "vmovntdq %%xmm0 ,     (%[dst])\n\t"
            :
            : [src] "r" (src), [dst] "r" (d)
            : "memory", "%xmm0", "%xmm1"
        );
        src = (__ivec4*)src + 1;
        d = (__ivec4*)d + 1;
        n -= 16;
    }
    return dest;
}

extern "C" void *memcpy32_nt_avx(void *dest, void const *src, size_t n)
{
    int *d = (int*)dest;
    int const *s = (int const *)src;
    while (n >= 64) {
        __builtin_ia32_movnti(&d[0],  s[0]);
        __builtin_ia32_movnti(&d[1],  s[1]);
        __builtin_ia32_movnti(&d[2],  s[2]);
        __builtin_ia32_movnti(&d[3],  s[3]);
        __builtin_ia32_movnti(&d[4],  s[4]);
        __builtin_ia32_movnti(&d[5],  s[5]);
        __builtin_ia32_movnti(&d[6],  s[6]);
        __builtin_ia32_movnti(&d[7],  s[7]);
        __builtin_ia32_movnti(&d[8],  s[8]);
        __builtin_ia32_movnti(&d[9],  s[9]);
        __builtin_ia32_movnti(&d[10], s[10]);
        __builtin_ia32_movnti(&d[11], s[11]);
        __builtin_ia32_movnti(&d[12], s[12]);
        __builtin_ia32_movnti(&d[13], s[13]);
        __builtin_ia32_movnti(&d[14], s[14]);
        __builtin_ia32_movnti(&d[15], s[15]);
        d += 16;
        s += 16;
        n -= 64;
    }
    while (n >= 4) {
        __builtin_ia32_movnti(d++, *s++);
        n -= 4;
    }
    return dest;
}

extern "C" void *memset32_nt_avx(void *dest, uint32_t val, size_t n)
{
    void *d = dest;

    // Write 32-bit values until it is 128-bit aligned
    while (((uintptr_t)d & 0xC) && n >= 4) {
        __builtin_ia32_movnti((int*)d, val);
        d = (uint32_t*)d + 1;
        n -= 4;
    }

    // Write as many 128-bit values as possible
    __uvec4 val128 = { val, val, val, val };
    while (n >= 16) {
        __builtin_ia32_movntdq((__ivec2LL*)d, (__ivec2LL)val128);
        d = (__ivec4*)d + 1;
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
