#pragma once
#include "types.h"
#include "type_traits.h"

// Return bit number of least significant set bit
_const
static constexpr _always_inline uint8_t bit_lsb_set_64(int64_t n)
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
static constexpr _always_inline uint8_t bit_lsb_set_32(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return __builtin_ctz(n);
#else
#error Unexpected integer sizes
#endif
}

// Return bit number of most significant set bit
_const
static constexpr _always_inline uint8_t bit_msb_set_64(int64_t n)
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
static constexpr _always_inline uint8_t bit_msb_set_32(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return 31 - __builtin_clz(n);
#else
#error Unexpected integer sizes
#endif
}

_const
static constexpr _always_inline uint8_t bit_popcnt_64(int64_t n)
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
static constexpr _always_inline uint8_t bit_popcnt_32(int32_t n)
{
#if __SIZEOF_INT__ == 4
    return __builtin_popcount(n);
#else
#error Unexpected integer sizes
#endif
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_lsb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_lsb_set_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_lsb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_lsb_set_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_lsb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_lsb_set_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_lsb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_lsb_set_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_lsb_set(T const& n)
{
    return bit_lsb_set_n(n, typename integral_constant<
                       uint8_t, sizeof(T)>::type());
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_msb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_msb_set_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_msb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_msb_set_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_msb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_msb_set_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_msb_set_n(
        T const& n, integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_msb_set_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_msb_set(T const& n)
{
    return bit_msb_set_n(n, typename integral_constant<
                         uint8_t, sizeof(n)>::type());
}

//
// ceil(log2(n))

// return ceil(log(n) / log(2))
_const
static constexpr _always_inline uint8_t bit_log2_n_64(int64_t n)
{
    uint8_t top = bit_msb_set_64(n);
    return top + !!(~(uint64_t(-1) << top) & n);
}

// return ceil(log(n) / log(2))
_const
static constexpr _always_inline uint8_t bit_log2_n_32(int32_t n)
{
    uint8_t top = bit_msb_set_32(n);
    return top + !!(~(uint32_t(-1) << top) & n);
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_log2_n(
        T const& n, integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_log2_n_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_log2_n(
        T const& n, integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_log2_n_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_log2_n(
        T const& n, integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_log2_n_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_log2_n(
        T const& n, integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_log2_n_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_log2(T const& n)
{
    return bit_log2_n(n, typename integral_constant<
                      uint8_t, sizeof(T)>::type());
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_popcnt_n(
        T const& n, integral_constant<uint8_t, sizeof(int64_t)>::type)
{
    return bit_popcnt_64(int64_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_popcnt_n(
        T const& n, integral_constant<uint8_t, sizeof(int32_t)>::type)
{
    return bit_popcnt_32(int32_t(n));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_popcnt_n(
        T const& n, integral_constant<uint8_t, sizeof(int16_t)>::type)
{
    return bit_popcnt_32(int32_t(uint16_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_popcnt_n(
        T const& n, integral_constant<uint8_t, sizeof(int8_t)>::type)
{
    return bit_popcnt_32(int32_t(uint8_t(n)));
}

template<typename T>
_const
static constexpr _always_inline uint8_t bit_popcnt(T const& n)
{
    return bit_popcnt_n(n, typename integral_constant<
                        uint8_t, sizeof(n)>::type());
}
