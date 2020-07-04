#include <stdio.h>
#include <sys/cdefs.h>
#include <stdint.h>
#include "frameserver.h"
#include <sys/framebuffer.h>
#include <immintrin.h>
#include <byteswap.h>
#include <string.h>
#include <png.h>
#include <sys/likely.h>
#include <sys/time.h>
#include <assert.h>

#define BLITTER_DEBUG 1
#if BLITTER_DEBUG

#define blitter_assert(e) assert(e)

#define assert_aligned(addr, alignment) \
    assert((uintptr_t(addr) & -(alignment)) == uintptr_t(addr))

#else

#define blitter_assert(e) assert(e) ((void)0)

#define assert_aligned(addr, alignment) ((void)0)

#endif

// Build a shuffle control mask consisting of 16 4-bit fields
// Same thing is broadcasted in AVX, this instruction only shuffles within
// 128-bit lanes
#define byte_reorder_shuffle(b, g, r, a) ( \
    (((UINT64_C(12)+b)|((12+g) << 4)|((12+r) << 8) | ((12+a) << 12)) << 48) | \
    (((UINT64_C(8 )+b)|((8 +g) << 4)|((8 +r) << 8) | ((8 +a) << 12)) << 32) | \
    (((UINT64_C(4 )+b)|((4 +g) << 4)|((4 +r) << 8) | ((4 +a) << 12)) << 16) | \
    (((UINT64_C(0 )+b)|((0 +g) << 4)|((0 +r) << 8) | ((0 +a) << 12)) << 0))

_optimized _ssse3
static void translate_pixels_rgbx32_ssse3(
        void * restrict output_p, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info)
{
    __m128i const shuf = _mm_cvtsi64_si128(
                byte_reorder_shuffle(2, 1, 0, 3));

    uint32_t * restrict output = (uint32_t*)output_p;

    assert_aligned(output, 4);

    if ((count >= 1) && (uintptr_t(output) & 0x07)) {
        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(*input), shuf)));

        output += 1;
        input += 1;
        count -= 1;
    }


    if ((count >= 2) && (uintptr_t(output) & 0x0F)) {
        assert_aligned(output, 8);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(
                                    *input | (uint64_t(input[1]) << 32)),
                            shuf)));

        output += 2;
        input += 2;
        count -= 2;
    }

    while (count >= 16) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input)),
                             shuf));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 4),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input + 4)),
                             shuf));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 8),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input + 8)),
                             shuf));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 12),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input + 12)),
                             shuf));

        input += 16;
        output += 16;
        count -= 16;
    }

    if (count >= 8) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input)),
                             shuf));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 4),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input + 4)),
                             shuf));

        input += 8;
        output += 8;
        count -= 8;
    }

    // Copy block of 4 pixels

    if (count >= 4) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(reinterpret_cast
                                             <__m128i_u const*>(input)),
                             shuf));

        input += 4;
        output += 4;
        count -= 4;
    }

    // Copy block of 2 pixels

    if (count >= 2) {
        assert_aligned(output, 8);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(
                                    *input | (uint64_t(input[1]) << 32)),
                            shuf)));

        output += 2;
        input += 2;
        count -= 2;
    }

    // Copy remaining pixel
    if (count >= 1) {
        assert_aligned(output, 4);

        _mm_stream_si32(reinterpret_cast<int*>(output), *input);

        output += 1;
        input += 1;
        count -= 1;
    }

    assert_aligned(output, 4);

    // Fence to push out fill buffers.
    // No point them hoping to be eventually filled
    _mm_sfence();
}

_avx2 _optimized
static void translate_pixels_rgbx32_avx2(
        void * restrict output_p, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info)
{
    __m256i const shuf = _mm256_broadcastsi128_si256(_mm_cvtsi64x_si128(
            ((UINT64_C(2) | (1U << 3) | (0U << 6) | (3U << 9)) << 36) |
            ((UINT64_C(2) | (1U << 3) | (0U << 6) | (3U << 9)) << 24) |
            ((UINT64_C(2) | (1U << 3) | (0U << 6) | (3U << 9)) << 12) |
            ((UINT64_C(2) | (1U << 3) | (0U << 6) | (3U << 9)) << 0)));

    uint32_t * restrict output = (uint32_t*)output_p;

    assert_aligned(output, 4);

    if ((count >= 1) && (uintptr_t(output) & 0x07)) {
        // low 32 bits of xmm register moved to integer register and
        // non-temporal stored from integer register
        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(*input),
                                _mm256_castsi256_si128(shuf))));

        output += 1;
        input += 1;
        count -= 1;

        // Now 8-byte aligned
    }

    if ((count >= 2) && (uintptr_t(output) & 0x0F)) {
        assert_aligned(output, 8);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(
                                    *input | (uint64_t(input[1]) << 32)),
                            _mm256_castsi256_si128(shuf))));

        output += 2;
        input += 2;
        count -= 2;

        // Now 16-byte aligned
    }

    // Either no pixels left, or destination is now 128-bit aligned

    if ((count >= 4) && (uintptr_t(output) & 0x10)) {
        assert_aligned(output, 16);

        __m128i pixels = _mm_loadu_si128(reinterpret_cast
                                         <__m128i_u const*>(input));

        pixels = _mm_shuffle_epi8(pixels, _mm256_castsi256_si128(shuf));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output), pixels);

        input += 4;
        output += 4;
        count -= 4;

        // Now 32-byte aligned
    }

    // Either no pixels left, or destination is now 256-bit aligned

    while (count >= 32) {
        assert_aligned(output, 32);

        // Copy 32 pixels

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input)),
                                shuf));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 8),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 8)),
                                shuf));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 16),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 16)),
                                shuf));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 24),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 24)),
                                shuf));

        input += 32;
        output += 32;
        count -= 32;
    }

    if (count >= 16) {
        assert_aligned(output, 32);

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input)),
                                shuf));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 8),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 8)),
                                shuf));

        input += 16;
        output += 16;
        count -= 16;
    }

    // Copy blocks of 8 pixels (up to 3 loops)
    if (count >= 8) {
        assert_aligned(output, 32);

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
                            _mm256_shuffle_epi8(
                                _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input)),
                                shuf));

        input += 8;
        output += 8;
        count -= 8;
    }

    // Copy blocks of 4 pixels
    if (count >= 4) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         _mm_shuffle_epi8(
                             _mm_loadu_si128(
                                 reinterpret_cast<__m128i_u const*>(input)),
                             _mm256_castsi256_si128(shuf)));

        input += 4;
        output += 4;
        count -= 4;
    }

    if (count >= 2) {
        assert_aligned(output, 8);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(
                                    *input | (uint64_t(input[1]) << 32)),
                             _mm256_castsi256_si128(shuf))));

        output += 2;
        input += 2;
        count -= 2;
    }

    // Copy remaining pixel
    if (count >= 1) {
        assert_aligned(output, 4);

        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(
                            _mm_shuffle_epi8(
                                _mm_cvtsi32_si128(*input),
                                _mm256_castsi256_si128(shuf))));

        output += 1;
        input += 1;
        count -= 1;
    }

    assert_aligned(output, 4);

    // Fence to push out fill buffers.
    // No point them hoping to be eventually filled
    _mm_sfence();
}

// It is much faster to have a dedicated version of this (and the sse one)
// in debug build. Calling the generic one with a noop translate
// block is much slower. Sorry for the duplication.
//_avx2 _optimized
//static void translate_pixels_bgrx32_avx2(
//        void * restrict output_p, uint32_t const * restrict input,
//        size_t count, fb_info_t const * restrict info)
//{
//    uint32_t * restrict output = (uint32_t*)output_p;

//    if ((count >= 1) && (uintptr_t(output) & 0x7)) {
//        // Destination is not 8-byte aligned
//        _mm_stream_si32(reinterpret_cast<int*>(output), *input);
//        output += 1;
//        input += 1;
//        count -= 1;
//    }

//    if ((count >= 2) && (uintptr_t(output) & 0xF)) {
//        // Destination is not 16-byte aligned
//        _mm_stream_si64(reinterpret_cast<long long*>(output),
//                        input[0] | (uint64_t(input[1]) << 32));
//        output += 2;
//        input += 2;
//        count -= 2;
//    }

//    if ((count >= 4) && (uintptr_t(output) & 0x1F)) {
//        // Destination is not 32-byte aligned
//        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input)));
//        output += 4;
//        input += 4;
//        count -= 4;
//    }

//    // Either no pixels left, or destination is now 256-bit aligned

//    while (count >= 32) {
//        // Copy 32 pixels

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input)));

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 8),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input + 8)));

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 16),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input + 16)));

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 24),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input + 24)));

//        input += 32;
//        output += 32;
//        count -= 32;
//    }

//    if (count >= 16) {
//        // Copy 16 pixels

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input)));

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 8),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input + 8)));

//        input += 16;
//        output += 16;
//        count -= 16;
//    }

//    if (count >= 8) {
//        // Copy 8 pixels

//        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
//                            _mm256_loadu_si256(reinterpret_cast
//                                               <__m256i_u const*>(input)));

//        input += 8;
//        output += 8;
//        count -= 8;
//    }

//    // Copy blocks of 4 pixels
//    if (count >= 4) {
//        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input)));

//        input += 4;
//        output += 4;
//        count -= 4;
//    }

//    if (count >= 2) {
//        _mm_stream_si64(reinterpret_cast<long long*>(output),
//                         input[0] | (uint64_t(input[1]) << 32));

//        input += 2;
//        output += 2;
//        count -= 2;
//    }

//    // Copy remaining pixels
//    if (count >= 1) {
//        _mm_stream_si32(reinterpret_cast<int*>(output), *input);

//        output += 1;
//        input += 1;
//        count -= 1;
//    }

//    // Fence to push out fill buffers.
//    // No point them hoping to be eventually filled
//    _mm_sfence();
//}

//_optimized
//static void translate_pixels_bgrx32_sse(
//        void * restrict output_p, uint32_t const * restrict input,
//        size_t count, fb_info_t const * restrict info)
//{
//    uint32_t * restrict output = (uint32_t*)output_p;

//    if ((count >= 1) && (uintptr_t(output) & 0x7)) {
//        // Destination is not 8-byte aligned
//        _mm_stream_si32(reinterpret_cast<int*>(output), *input);

//        output += 1;
//        input += 1;
//        count -= 1;
//    }

//    if ((count >= 2) && (uintptr_t(output) & 0xF)) {
//        // Destination is not 16-byte aligned
//        _mm_stream_si64(reinterpret_cast<long long*>(output),
//                        input[0] | ((uint64_t(input[1]) << 32)));

//        output += 2;
//        input += 2;
//        count -= 2;
//    }

//    while (count >= 16) {
//        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input)));

//        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 4),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input + 4)));

//        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 8),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input + 8)));

//        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 12),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input + 12)));

//        input += 16;
//        output += 16;
//        count -= 16;
//    }

//    // Copy block of 8 pixels
//    if (count >= 8) {
//        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input)));

//        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 4),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input + 4)));

//        input += 8;
//        output += 8;
//        count -= 8;
//    }

//    // Copy block of 4 pixels
//    if (count >= 4) {
//        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
//                         _mm_loadu_si128(reinterpret_cast
//                                         <__m128i_u const*>(input)));

//        input += 4;
//        output += 4;
//        count -= 4;
//    }

//    // Copy block of 2 pixels
//    if (count >= 2) {
//        // Destination is not 16-byte aligned
//        _mm_stream_si64(reinterpret_cast<long long*>(output),
//                        input[0] | ((uint64_t(input[1]) << 32)));

//        output += 2;
//        input += 2;
//        count -= 2;
//    }

//    // Copy remaining pixels
//    if (count >= 1) {
//        _mm_stream_si32(reinterpret_cast<int*>(output), *input);

//        output += 1;
//        input += 1;
//        count -= 1;
//    }

//    // Fence to push out fill buffers.
//    // No point them hoping to be eventually filled
//    _mm_sfence();
//}

template<int rbit, int gbit, int bbit, int abit,
         int rshl, int gshl, int bshl, int ashl>
_optimized
static __m128i translate_block_specific_sse(
        fb_info_t const * restrict , __m128i pixels)
{
    __m128i mask = _mm_set1_epi32(0xFF);

    __m128i b = pixels;
    __m128i g = _mm_srli_epi32(pixels, 8);
    __m128i r = _mm_srli_epi32(pixels, 16);
    __m128i a = _mm_srli_epi32(pixels, 24);

    b = _mm_and_si128(b, mask);
    g = _mm_and_si128(g, mask);
    r = _mm_and_si128(r, mask);
    a = _mm_and_si128(a, mask);

    if (bbit)
        b = _mm_srli_epi32(b, 8 - bbit);
    if (gbit)
        g = _mm_srli_epi32(g, 8 - gbit);
    if (rbit)
        r = _mm_srli_epi32(r, 8 - rbit);
    if (abit)
        a = _mm_srli_epi32(a, 8 - abit);

    if (bshl)
        b = _mm_slli_epi32(b, bshl);
    if (gshl)
        g = _mm_slli_epi32(g, gshl);
    if (rshl)
        r = _mm_slli_epi32(r, rshl);
    if (ashl)
        a = _mm_slli_epi32(a, ashl);

    b = _mm_or_si128(b, g);
    r = _mm_or_si128(r, a);
    b = _mm_or_si128(b, r);

    return b;
}

_optimized
static __m128i translate_block_noop_sse(
        fb_info_t const * restrict , __m128i pixels)
{
    return pixels;
}

template<int rshr, int gshr, int bshr, int ashr,
         int rshl, int gshl, int bshl, int ashl>
_avx2 _optimized
static __m256i translate_block_specific_avx2(
        fb_info_t const * restrict, __m256i pixels)
{
    __m256i mask = _mm256_set1_epi32(0xFF);

    __m256i b = pixels;
    __m256i g = _mm256_srli_epi32(pixels, 8);
    __m256i r = _mm256_srli_epi32(pixels, 16);
    __m256i a = _mm256_srli_epi32(pixels, 24);

    b = _mm256_and_si256(b, mask);
    g = _mm256_and_si256(g, mask);
    r = _mm256_and_si256(r, mask);
    a = _mm256_and_si256(a, mask);

    if (bshr)
        b = _mm256_srli_epi32(r, bshr);
    if (gshr)
        g = _mm256_srli_epi32(r, gshr);
    if (rshr)
        r = _mm256_srli_epi32(r, rshr);
    if (ashr)
        a = _mm256_srli_epi32(r, ashr);

    if (bshl)
        b = _mm256_slli_epi32(r, bshl);
    if (gshl)
        g = _mm256_slli_epi32(r, gshl);
    if (rshl)
        r = _mm256_slli_epi32(r, rshl);
    if (ashl)
        a = _mm256_slli_epi32(r, ashl);

    b = _mm256_or_si256(b, g);
    r = _mm256_or_si256(r, a);
    b = _mm256_or_si256(b, r);

    return b;
}

_avx2
static __m256i translate_block_noop_avx2(fb_info_t const * restrict,
                                         __m256i pixels)
{
    return pixels;
}

_optimized static __m128i translate_block_generic_sse(
        fb_info_t const * restrict info, __m128i pixels)
{
    __m128i mask = _mm_set1_epi32(0xFF);

    __m128i b = pixels;
    __m128i g = _mm_srli_epi32(pixels, 8);
    __m128i r = _mm_srli_epi32(pixels, 16);
    __m128i a = _mm_srli_epi32(pixels, 24);

    r = _mm_and_si128(r, mask);
    g = _mm_and_si128(g, mask);
    b = _mm_and_si128(b, mask);
    a = _mm_and_si128(a, mask);

    r = _mm_srl_epi32(r, _mm_cvtsi32_si128(8 - info->fmt.mask_size_r));
    g = _mm_srl_epi32(g, _mm_cvtsi32_si128(8 - info->fmt.mask_size_g));
    b = _mm_srl_epi32(b, _mm_cvtsi32_si128(8 - info->fmt.mask_size_b));
    a = _mm_srl_epi32(a, _mm_cvtsi32_si128(8 - info->fmt.mask_size_a));

    r = _mm_sll_epi32(r, _mm_cvtsi32_si128(info->fmt.mask_pos_r));
    g = _mm_sll_epi32(g, _mm_cvtsi32_si128(info->fmt.mask_pos_g));
    b = _mm_sll_epi32(b, _mm_cvtsi32_si128(info->fmt.mask_pos_b));
    a = _mm_sll_epi32(a, _mm_cvtsi32_si128(info->fmt.mask_pos_a));

    r = _mm_or_si128(r, g);
    b = _mm_or_si128(b, a);
    r = _mm_or_si128(r, b);

    return r;
}

_avx2 _optimized
static __m256i translate_block_generic_avx2(
        fb_info_t const *info, __m256i pixels)
{
    __m256i mask = _mm256_set1_epi32(0xFF);

    __m256i b = pixels;
    __m256i g = _mm256_srli_epi32(pixels, 8);
    __m256i r = _mm256_srli_epi32(pixels, 16);
    __m256i a = _mm256_srli_epi32(pixels, 24);

    r = _mm256_and_si256(r, mask);
    g = _mm256_and_si256(g, mask);
    b = _mm256_and_si256(b, mask);
    a = _mm256_and_si256(a, mask);

    r = _mm256_srl_epi32(r, _mm_cvtsi32_si128(8 - info->fmt.mask_size_r));
    g = _mm256_srl_epi32(r, _mm_cvtsi32_si128(8 - info->fmt.mask_size_g));
    b = _mm256_srl_epi32(r, _mm_cvtsi32_si128(8 - info->fmt.mask_size_b));
    a = _mm256_srl_epi32(r, _mm_cvtsi32_si128(8 - info->fmt.mask_size_a));

    r = _mm256_sll_epi32(r, _mm_cvtsi32_si128(info->fmt.mask_pos_r));
    g = _mm256_sll_epi32(r, _mm_cvtsi32_si128(info->fmt.mask_pos_g));
    b = _mm256_sll_epi32(r, _mm_cvtsi32_si128(info->fmt.mask_pos_b));
    a = _mm256_sll_epi32(r, _mm_cvtsi32_si128(info->fmt.mask_pos_a));

    r = _mm256_or_si256(r, g);
    b = _mm256_or_si256(b, a);
    r = _mm256_or_si256(r, b);

    return r;
}

_optimized


typedef void (*translate_pixels_fn)(
        void * restrict output, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info);

translate_pixels_fn translate_pixels;

static bool fmt_is_bgrx32(pix_fmt_t const& fmt)
{
    return fmt.mask_pos_r == 16 &&
            fmt.mask_pos_g == 8 &&
            fmt.mask_pos_b == 0 &&
            fmt.mask_pos_a == 24 &&
            fmt.mask_size_r == 8 &&
            fmt.mask_size_g == 8 &&
            fmt.mask_size_b == 8 &&
            fmt.mask_size_a == 8;
}

static bool fmt_is_rgbx32(pix_fmt_t fmt)
{
    return fmt.mask_pos_r == 0 &&
            fmt.mask_pos_g == 8 &&
            fmt.mask_pos_b == 16 &&
            fmt.mask_pos_a == 24 &&
            fmt.mask_size_r == 8 &&
            fmt.mask_size_g == 8 &&
            fmt.mask_size_b == 8 &&
            fmt.mask_size_a == 8;
}

static bool fmt_is_rgb_565_16(pix_fmt_t fmt)
{
    return fmt.mask_pos_r == 0 &&
            fmt.mask_pos_g == 5 &&
            fmt.mask_pos_b == 11 &&
            fmt.mask_size_r == 5 &&
            fmt.mask_size_g == 6 &&
            fmt.mask_size_b == 5 &&
            fmt.mask_size_a == 0;
}

static bool fmt_is_rgba_1555_16(pix_fmt_t fmt)
{
    return fmt.mask_size_r == 5 &&
            fmt.mask_size_g == 5 &&
            fmt.mask_size_b == 5 &&
            fmt.mask_size_a == 1 &&
            fmt.mask_pos_r == 10 &&
            fmt.mask_pos_g == 5 &&
            fmt.mask_pos_b == 0 &&
            fmt.mask_pos_a == 15;
}

template<__m128i (*translate_block)(fb_info_t const * restrict, __m128i)>
_optimized
static void translate_pixels_generic32_sse(
        void * restrict output_p, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info)
{
    uint32_t * restrict output = (uint32_t*)output_p;

    assert_aligned(output, 4);

    if ((count >= 1) && (uintptr_t(output) & 0x07)) {
        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(
                            translate_block(
                                info, _mm_cvtsi32_si128(*input))));

        output += 1;
        input += 1;
        count -= 1;
    }


    if ((count >= 2) && (uintptr_t(output) & 0x0F)) {
        assert_aligned(output, 8);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si64(
                            translate_block(
                                info, _mm_cvtsi64_si128(
                                    input[0] | (uint64_t(input[1]) << 32)))));

        output += 2;
        input += 2;
        count -= 2;
    }

    // Either no pixels left, or destination is now 128-bit aligned

    while (count >= 16) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input))));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 4),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input + 4))));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 8),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input + 8))));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 12),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input + 12))));

        input += 16;
        output += 16;
        count -= 16;
    }

    if (count >= 8) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input))));

        _mm_stream_si128(reinterpret_cast<__m128i*>(output + 4),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input + 4))));

        input += 8;
        output += 8;
        count -= 8;
    }

    // Copy blocks of 4 pixels
    while (count >= 4) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         translate_block(
                             info, _mm_loadu_si128(
                                 reinterpret_cast
                                 <__m128i_u const*>(input))));

        input += 4;
        output += 4;
        count -= 4;
    }


    if (count >= 2) {
        assert_aligned(output, 16);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si64(
                            translate_block(
                                info, _mm_cvtsi64_si128(
                                    input[0] | (uint64_t(input[1]) << 32)))));

        input += 2;
        output += 2;
        count -= 2;
    }

    // Copy remaining pixel

    if (count == 1) {
        assert_aligned(output, 8);

        _mm_stream_si32(reinterpret_cast<int*>(output++),
                        _mm_cvtsi128_si32(
                            translate_block(
                                info, _mm_cvtsi32_si128(*input))));

        input += 1;
        output += 1;
        count -= 1;
    }


    assert_aligned(output, 4);

    // Fence to push out fill buffers.
    // No point them hoping to be eventually filled
    _mm_sfence();
}

template<__m256i (*translate_block)(fb_info_t const * restrict, __m256i)>
_avx2 _optimized
static void translate_pixels_generic32_avx2(
        void * restrict output_p, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info)
{
    uint32_t * restrict output = (uint32_t*)output_p;

    assert_aligned(output, 4);

    if ((count >= 1) && (uintptr_t(output) & 0x7)) {
        // Destination is not 8-byte aligned
        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(
                            _mm256_castsi256_si128(
                                translate_block(
                                    info, _mm256_castsi128_si256(
                                        _mm_cvtsi32_si128(*input))))));

        output += 1;
        input += 1;
        count -= 1;
    }

    if ((count >= 2) && (uintptr_t(output) & 0xF)) {
        assert_aligned(output, 8);

        // Destination is not 16-byte aligned
        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si64(
                            _mm256_castsi256_si128(
                                translate_block(
                                    info, _mm256_castsi128_si256(
                                        _mm_cvtsi64_si128(
                                            input[0] |
                                            (uint64_t(input[1]) << 32)))))));

        output += 2;
        input += 2;
        count -= 2;
    }

    if ((count >= 4) && (uintptr_t(output) & 0x1F)) {
        assert_aligned(output, 16);

        // Destination is not 32-byte aligned
        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         _mm256_castsi256_si128(
                             translate_block(
                                 info, _mm256_castsi128_si256(
                                     _mm_loadu_si128(
                                         reinterpret_cast
                                         <__m128i_u const*>(input))))));

        output += 4;
        input += 4;
        count -= 4;
    }

    // Copy blocks of 32 pixels
    while (count >= 32) {
        assert_aligned(output, 32);

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input))));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 8),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 8))));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 16),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 16))));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 24),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 24))));

        input += 32;
        output += 32;
        count -= 32;
    }

    // Copy block of 16 pixels
    if (count >= 16) {
        assert_aligned(output, 32);

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input))));

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output + 8),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input + 8))));

        input += 16;
        output += 16;
        count -= 16;
    }

    // Copy block of 8 pixels
    if (count >= 8) {
        assert_aligned(output, 32);

        _mm256_stream_si256(reinterpret_cast<__m256i*>(output),
                            translate_block(
                                info, _mm256_loadu_si256(
                                    reinterpret_cast
                                    <__m256i_u const*>(input))));

        input += 8;
        output += 8;
        count -= 8;
    }

    // Copy blocks of 4 pixels

    if (count >= 4) {
        assert_aligned(output, 16);

        _mm_stream_si128(reinterpret_cast<__m128i*>(output),
                         _mm256_castsi256_si128(
                             translate_block(
                                 info, _mm256_castsi128_si256(
                                     _mm_loadu_si128(
                                         reinterpret_cast
                                         <__m128i_u const*>(input))))));

        input += 4;
        output += 4;
        count -= 4;
    }


    // Copy block of 2 pixels
    if (count >= 2) {
        assert_aligned(output, 8);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si64(
                            _mm256_castsi256_si128(
                                translate_block(
                                    info, _mm256_castsi128_si256(
                                        _mm_cvtsi64_si128(
                                            input[0] |
                                            (uint64_t(input[1]) << 32)))))));

        input += 2;
        output += 2;
        count -= 2;
    }

    // Copy remaining pixels
    if (count >= 1) {
        assert_aligned(output, 4);

        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(
                            _mm256_castsi256_si128(
                                translate_block(
                                    info, _mm256_castsi128_si256(
                                        _mm_cvtsi32_si128(*input))))));

        input += 1;
        output += 1;
        count -= 1;
    }

    assert_aligned(output, 4);

    // Fence to push out fill buffers.
    // No point them hoping to be eventually filled
    _mm_sfence();
}

template<__m128i (*translate_block)(fb_info_t const * restrict, __m128i)>
_optimized _sse4_1
static void translate_pixels_generic16_sse4_1(
        void * restrict output_p, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info)
{
    uint16_t * restrict output = (uint16_t*)output_p;

    // destination is at least 16-bit aligned...

    assert_aligned(output, 2);

    if ((count >= 1) && (uintptr_t(output) & 3)) {
        // Move 1 pixel because destination is not 32-bit aligned
        *output = (uint16_t)_mm_cvtsi128_si32(
                    translate_block(info, _mm_cvtsi32_si128(input[0])));

        input += 1;
        output += 1;
        count -= 1;

        // Destination is now 32 bit aligned
    }

    if ((count >= 2) && (uintptr_t(output) & 7)) {
        assert_aligned(output, 4);

        // Move 2 pixels because destination is not 64-bit aligned
        __m128i pixels = _mm_unpacklo_epi32(
                    _mm_cvtsi32_si128(input[0]),
                    _mm_cvtsi32_si128(input[1]));

        pixels = translate_block(info, pixels);

        pixels = _mm_packus_epi32(pixels, pixels);

        _mm_stream_si32(reinterpret_cast<int32_t*>(output),
                        _mm_cvtsi128_si32(pixels));

        input += 2;
        output += 2;
        count -= 2;

        // destination is now 64-bit aligned
    }


    if ((count >= 4) && (uintptr_t(output) & 15)) {
        assert_aligned(output, 8);

        // Move 4 pixels because destination is not 128-bit aligned
        __m128i pixels = _mm_loadu_si128(reinterpret_cast
                                         <__m128i_u const *>(input));

        pixels = translate_block(info, pixels);
        pixels = _mm_packus_epi32(pixels, pixels);

        _mm_stream_si64(reinterpret_cast<long long*>(output),
                        _mm_cvtsi128_si64(pixels));

        input += 4;
        output += 4;
        count -= 4;

        // destination is now 128-bit aligned
    }

    while (count >= 8) {
        assert_aligned(output, 16);

        // Move 8 pixels (2 loads, 1 store)

        __m128i pixels = _mm_loadu_si128(reinterpret_cast
                                         <__m128i_u const*>(input));
        __m128i pixels2 = _mm_loadu_si128(reinterpret_cast
                                          <__m128i_u const*>(input + 4));

        pixels = translate_block(info, pixels);
        pixels2 = translate_block(info, pixels2);

        // Pack down to 16 bits per pixel
        pixels = _mm_packus_epi32(pixels, pixels);
        pixels2 = _mm_packus_epi32(pixels2, pixels2);

        pixels = _mm_unpacklo_epi64(pixels, pixels2);
        _mm_stream_si128(reinterpret_cast<__m128i*>(output), pixels);

        input += 8;
        output += 8;
        count -= 8;
    }

    // Copy blocks of 4 pixels
    if (count >= 4) {
        assert_aligned(output, 8);

        __m128i pixels = _mm_loadu_si128(reinterpret_cast
                                         <__m128i_u const*>(input));
        pixels = translate_block(info, pixels);
        _mm_stream_si64(reinterpret_cast<long long *>(output),
                        _mm_cvtsi128_si64(pixels));
        input += 4;
        output += 4;
        count -= 4;
    }

    // Copy blocks of 2 pixels
    if (count >= 2) {
        assert_aligned(output, 4);

        // Move two pixels because destination is not 64-bit aligned
        __m128i pixels = _mm_unpacklo_epi32(_mm_cvtsi32_si128(*input),
                                            _mm_cvtsi32_si128(input[1]));

        pixels = translate_block(info, pixels);

        _mm_stream_si32(reinterpret_cast<int*>(output),
                        _mm_cvtsi128_si32(pixels));

        input += 2;
        output += 2;
        count -= 2;
    }

    // Copy remaining pixel
    if (count >= 1) {
        assert_aligned(output, 2);

        *(uint16_t*)output = (uint16_t)
                _mm_cvtsi128_si32(
                    translate_block(
                        info, _mm_cvtsi32_si128(*input)));

        input += 1;
        output += 1;
        count -= 1;
    }

    assert_aligned(output, 2);

    // Fence to push out fill buffers.
    // No point them hoping to be eventually filled
    _mm_sfence();
}

_optimized
static void translate_pixels_generic(
        void * restrict output, uint32_t const * restrict input,
        size_t count, fb_info_t const * restrict info)
{
    while (count--) {
        uint32_t pixel = *input++;
        uint32_t b= (pixel) & 0xFF;
        uint32_t g = (pixel >> 8) & 0xFF;
        uint32_t r = (pixel >> 16) & 0xFF;
        uint32_t a = (pixel >> 24) & 0xFF;

        b >>= 8 - info->fmt.mask_size_b;
        g >>= 8 - info->fmt.mask_size_g;
        r >>= 8 - info->fmt.mask_size_r;
        a >>= 8 - info->fmt.mask_size_a;

        b <<= info->fmt.mask_pos_b;
        g <<= info->fmt.mask_pos_g;
        r <<= info->fmt.mask_pos_r;
        a <<= info->fmt.mask_pos_a;

        b |= g;
        r |= a;
        b |= r;

        memcpy(output, &b, info->pixel_sz);
        output = (char*)output + info->pixel_sz;
    }
}

static translate_pixels_fn translate_pixels_resolver(fb_info_t *info)
{
    __builtin_cpu_init();

    bool avx2 = __builtin_cpu_supports("avx2");

    // Good vector shuffle for 16 separate bytes
    bool ssse3 = __builtin_cpu_supports("ssse3");

    bool sse4_1 = __builtin_cpu_supports("sse4.1");

    if (fmt_is_bgrx32(info->fmt)) {
        // Fastest

        if (avx2)
            return translate_pixels_generic32_avx2<
                    translate_block_noop_avx2>;

        return translate_pixels_generic32_sse<
                translate_block_noop_sse>;
    }

    if (fmt_is_rgbx32(info->fmt)) {
        if (avx2)
            return translate_pixels_rgbx32_avx2;

        if (ssse3)
            return translate_pixels_rgbx32_ssse3;
    }

    if (info->pixel_sz == 4) {
        if (avx2)
            return translate_pixels_generic32_avx2
                    <translate_block_generic_avx2>;

        return translate_pixels_generic32_sse
                <translate_block_generic_sse>;
    }

    if (info->pixel_sz == 2 && sse4_1) {
        // Note: discarding alpha channel below

        if (fmt_is_rgba_1555_16(info->fmt))
            return translate_pixels_generic16_sse4_1
                    <translate_block_specific_sse<5, 5, 5, 1, 10, 5, 0, 15>>;

        if (fmt_is_rgb_565_16(info->fmt))
            return translate_pixels_generic16_sse4_1
                    <translate_block_specific_sse<5, 6, 5, 0, 0, 5, 10, 0>>;

        return translate_pixels_generic16_sse4_1<translate_block_generic_sse>;
    }

    // Let the rest go to the unoptimized generic scalar implementation

    return translate_pixels_generic;
}

_optimized
void surface_draw_noclip(int dx, int dy,
                         int dw, int dh,
                         int sx, int sy,
                         surface_t const *img, fb_info_t const *info)
{
    // Calculate a pointer to the first image pixel
    uint32_t const * restrict src = surface_pixels(img) + img->width * sy + sx;

    void * restrict dst;
    dst = ((char*)info->vmem + dy * info->pitch) + dx * info->pixel_sz;

    for (size_t y = 0; y < unsigned(dh); ++y) {
        translate_pixels(dst, src, dw, info);
        src += img->width;
        dst = (char*)dst + info->pitch;
    }
}

_optimized
void surface_draw(int dx, int dy,
                  int dw, int dh,
                  int sx, int sy,
                  surface_t const *img, fb_info_t const *info)
{
    int hclip, vclip;

    // Calculate amount clipped off left and top
    hclip = -dx;
    vclip = -dy;

    // Zero out negative clip amount
    hclip &= -(hclip >= 0);
    vclip &= -(vclip >= 0);

    // Adjust left and width and source left
    dx += hclip;
    sx += hclip;
    dw -= hclip;

    // Adjust top and height and source top
    dy += vclip;
    sy += vclip;
    dh -= vclip;

    // Calculate amount clipped off right and bottom
    hclip = (dx + dw) - info->w;
    vclip = (dy + dh) - info->h;

    // Zero out negative clip amount
    hclip &= -(hclip >= 0);
    vclip &= -(vclip >= 0);

    // Adjust bottom of destination to handle clip
    dw -= hclip;
    dh -= vclip;

    // Clamp right and bottom of destination
    // to right and bottom edge of source
    hclip = (sx + dw) - img->width;
    vclip = (sy + dh) - img->height;

    // Zero out negative clip amount
    hclip &= -(hclip >= 0);
    vclip &= -(vclip >= 0);

    // Adjust bottom of destination to handle clip
    dw -= hclip;
    dh -= vclip;

    if (unlikely((dw <= 0) | (dh <= 0)))
        return;

    surface_draw_noclip(dx, dy, dw, dh, sx, sy, img, info);
}

static int stress(fb_info_t const& info)
{
    surface_t *img = surface_from_png("background.png");

    if (unlikely(!img))
        return -1;

    uint64_t const ns_per_sec = UINT64_C(1000000000);
    int x = 0;
    int direction = 1;
    int divisor = 10;   // 10 frames average for initial divisor recalculation
    int divisor_countdown = divisor;
    timespec since{};
    clock_gettime(CLOCK_MONOTONIC, &since);
    for (int frame_count = 0; ; ++frame_count) {
        if (--divisor_countdown == 0) {
            timespec now{};
            clock_gettime(CLOCK_MONOTONIC, &now);

            int64_t since_time = since.tv_sec * ns_per_sec +
                    since.tv_nsec;

            int64_t now_time = now.tv_sec * ns_per_sec +
                    now.tv_nsec;

            int64_t elap_time = now_time - since_time;

            int64_t ns_per_frame = elap_time / divisor;

            int64_t fps = ns_per_sec / ns_per_frame;

            printf("blitter stress, %u.%05u fps=%zu\n",
                   (unsigned)now.tv_sec, unsigned(now.tv_nsec / 10000),
                   (size_t)fps);

            // Readjust divisor each update,
            /// so we adaptively do this once per second
            divisor = fps;
            divisor_countdown = divisor;

            memcpy(&since, &now, sizeof(since));
        }

        x += direction;

        int clip_edges = 0;

        if (x == -info.w * clip_edges)
            direction = 1;

        if (x - info.w * clip_edges >= img->width - info.w ||
                x - info.h * clip_edges >= img->height - info.h)
            direction = -1;

        surface_draw(-x, -x, img->width, img->height, 0, 0, img, &info);
    }

    surface_free(img);

    return 0;
}

int start_framebuffer()
{
    fb_info_t info;
    int err = framebuffer_enum(0, 0, &info);

    if (unlikely(err < 0)) {
        errno_t save_errno = errno;
        printf("Unable to map framebuffer!\n");
        errno = save_errno;
        return -1;
    }

    printf("Using %dx%d %zdbpp framebuffer\n",
           info.w, info.h, info.pixel_sz * 8);

    translate_pixels = translate_pixels_resolver(&info);

    return stress(info);
}
