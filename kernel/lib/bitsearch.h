#pragma once
#include "types.h"
#include "cc/type_traits.h"
#include "assert.h"

// Return bit number of least significant set bit
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set_64(int64_t n)
{
#if __SIZEOF_LONG__ == 8
    return __builtin_ctzl(n);
#elif __SIZEOF_LONG_LONG__ == 8
    return __builtin_ctzll(n);
#else
#error Unexpected integer sizes
#endif
}

// Return bit number of least significant set bit
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set_32(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return __builtin_ctz(n);
#else
#error Unexpected integer sizes
#endif
}

// Return bit number of most significant set bit
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set_64(int64_t n)
{
#if __SIZEOF_LONG__ == 8
    return 63 - __builtin_clzl(n);
#elif __SIZEOF_LONG_LONG__ == 8
    return 63 - __builtin_clzll(n);
#else
#error Unexpected integer sizes
#endif
}

// Return bit number of most significant set bit
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set_32(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return 31 - __builtin_clz(n);
#else
#error Unexpected integer sizes
#endif
}

// Return bit number of most significant set bit
_const
static constexpr _always_inline _flatten uint8_t bit_clz(int64_t n)
{
#if __SIZEOF_LONG__ == 8
    return __builtin_clzl(n);
#elif __SIZEOF_LONG_LONG__ == 8
    return __builtin_clzll(n);
#else
#error Unexpected integer sizes
#endif
}

// Return bit number of most significant set bit
_const
static constexpr _always_inline _flatten uint8_t bit_clz(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return __builtin_clz(n);
#else
#error Unexpected integer sizes
#endif
}

_const
static constexpr _always_inline _flatten uint8_t bit_popcnt_64(int64_t n)
{
#if __SIZEOF_LONG__ == 8
    return __builtin_popcountl(n);
#elif __SIZEOF_LONG_LONG__ == 8
    return __builtin_popcountll(n);
#else
#error Unexpected integer sizes
#endif
}

_const
static constexpr _always_inline _flatten uint8_t bit_popcnt_32(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return __builtin_popcount(n);
#else
#error Unexpected integer sizes
#endif
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_lsb_set_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_lsb_set_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_lsb_set_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_lsb_set_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_lsb_set(T const& n)
{
    return bit_lsb_set_n(n, typename std::integral_constant<
                       uint8_t, sizeof(T)>::type());
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_msb_set_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_msb_set_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_msb_set_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_msb_set_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_msb_set(T const& n)
{
    return bit_msb_set_n(n, typename std::integral_constant<
                         uint8_t, sizeof(n)>::type());
}

//
// ceil(log2(n))

// return ceil(log(n) / log(2))
_const
static constexpr _always_inline _flatten uint8_t bit_log2_n_64(int64_t n)
{
    uint8_t top = bit_msb_set_64(n);
    return top + !!(~(~UINT64_C(0) << top) & n);
}

// return ceil(log(n) / log(2))
_const
static constexpr _always_inline _flatten uint8_t bit_log2_n_32(int32_t n)
{
    uint8_t top = bit_msb_set_32(n);
    return top + !!(~(~UINT32_C(0) << top) & n);
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_log2_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_log2_n_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_log2_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_log2_n_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_log2_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_log2_n_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_log2_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_log2_n_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_log2(T const& n)
{
    assert(n != 0);
    return bit_log2_n(n, typename std::integral_constant<
                      uint8_t, sizeof(T)>::type());
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_popcnt_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_popcnt_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_popcnt_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_popcnt_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_popcnt_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_popcnt_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_popcnt_n(
        T const& n, std::integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_popcnt_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline _flatten uint8_t bit_popcnt(T const& n)
{
    return bit_popcnt_n(n, typename std::integral_constant<
                        uint8_t, sizeof(n)>::type());
}

template<size_t _D>
class bitmap_tree_impl_t {
public:
    // depth     max capacity              min capacity
    // ----- --------------------- -------------------------
    //   1            64 (2^(6*1))                         1
    //   2          4096 (2^(6*2))                      64+1
    //   3        262144 (2^(6*3))                 4096+64+1
    //   4      16777216 (2^(6*4))          262144+4096+64+1
    //   5    1073741824 (2^(6*5)) 16777216+262144+4096+64+1

    static_assert(sizeof(uint64_t) == 8, "");

    static constexpr const size_t depth = _D;
    static constexpr const size_t capacity = 1 << (depth * 6);
    //static constexpr const size_t slots = infer_slot_count;

private:
};

static inline constexpr size_t bitmap_tree_depth(size_t n)
{
    for (size_t d = 1, c = 64; d < 6; ++d, c <<= 6) {
        if (c >= n)
            return d;
    }
    return 0;
}

template<size_t _N>
using bitmap_tree_t = bitmap_tree_impl_t<bitmap_tree_depth(_N)>;
