#pragma once
#include "types.h"
#include "type_traits.h"

// Return bit number of least significant set bit
__const
static constexpr inline uint8_t bit_lsb_set_32(int32_t n)
{
    return __builtin_ctz(n);
}

// Return bit number of least significant set bit
__const
static constexpr inline uint8_t bit_lsb_set_64(int64_t n)
{
    return __builtin_ctzl(n);
}

// Return bit number of most significant set bit
__const
static constexpr inline uint8_t bit_msb_set_32(int32_t n)
{
    return 31 - __builtin_clz(n);
}

// Return bit number of most significant set bit
__const
static constexpr inline uint8_t bit_msb_set_64(int64_t n)
{
    return 63 - __builtin_clzl(n);
}

template<typename T>
__const
static constexpr inline uint8_t bit_lsb_set(T n, false_type)
{
    return bit_lsb_set_32(int32_t(n));
}

template<typename T>
__const
static constexpr inline uint8_t bit_lsb_set(T n, true_type)
{
    return bit_lsb_set_64(int64_t(n));
}

template<typename T>
__const
static constexpr inline uint8_t bit_lsb_set(T n)
{
    return bit_lsb_set(n, typename conditional<
                       sizeof(T) == sizeof(uint64_t),
                       true_type, false_type>::type());
}

template<typename T>
__const
static constexpr inline uint8_t bit_msb_set(T n, false_type)
{
    return bit_msb_set_32(int32_t(n));
}

template<typename T>
__const
static constexpr inline uint8_t bit_msb_set(T n, true_type)
{
    return bit_msb_set_64(int64_t(n));
}

template<typename T>
__const
static constexpr inline uint8_t bit_msb_set(T n)
{
    return bit_msb_set(n, typename conditional<
                       sizeof(T) == sizeof(uint64_t),
                       true_type, false_type>::type());
}

// return ceil(log(n) / log(2))
__const
static constexpr inline uint8_t bit_log2_n_32(int32_t n)
{
    uint8_t top = bit_msb_set_32(n);
    return top + !!(~(uint32_t(-1) << top) & n);
}

// return ceil(log(n) / log(2))
__const
static constexpr inline uint8_t bit_log2_n_64(int64_t n)
{
    uint8_t top = bit_msb_set_64(n);
    return top + !!(~(uint64_t(-1) << top) & n);
}

template<typename T>
__const
static constexpr inline uint8_t bit_log2_n(T n, false_type)
{
    return bit_log2_n_32(int32_t(n));
}

template<typename T>
__const
static constexpr inline uint8_t bit_log2_n(T n, true_type)
{
    return bit_log2_n_64(int64_t(n));
}

template<typename T>
__const
static constexpr inline uint8_t bit_log2_n(T n)
{
    return bit_log2_n(n, typename conditional<
                      sizeof(T) == sizeof(uint64_t),
                      true_type, false_type>::type());
}
