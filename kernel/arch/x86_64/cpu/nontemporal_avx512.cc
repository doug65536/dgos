#include "types.h"
#include "likely.h"

extern "C" void *memcpy512_nt_avx512(void *dest, void const *src, size_t n)
{
    void *d = dest;
    while (n >= 256) {
        __asm__ __volatile__ (
            "vmovntdqa      (%[src]),%%zmm0\n\t"
            "vmovntdqa  4*16(%[src]),%%zmm1\n\t"
            "vmovntdqa  8*16(%[src]),%%zmm2\n\t"
            "vmovntdqa 12*16(%[src]),%%zmm3\n\t"
            "vmovntdq %%zmm0 ,     (%[dst])\n\t"
            "vmovntdq %%zmm1 , 4*16(%[dst])\n\t"
            "vmovntdq %%zmm2 , 8*16(%[dst])\n\t"
            "vmovntdq %%zmm3 ,12*16(%[dst])\n\t"
            :
            : [src] "r" (src), [dst] "r" (d)
            : "memory", "%ymm0", "%ymm1"
        );
        src = (__ivec4*)src + 16;
        d = (__ivec4*)d + 16;
        n -= 256;
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
            "vmovntdqa      (%[src]),%%xmm0\n\t"
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
