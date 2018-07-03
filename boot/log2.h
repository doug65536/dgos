#pragma once

template<typename T, T n>
struct integral_constant
{
    static constexpr T value = n;
    using type = integral_constant<T, n>;
};

#define clz_32 __builtin_clz
#if __SIZEOF_LONG__ == 8
#define clz_64 __builtin_clzl
#elif __SIZEOF_LONG_LONG__ == 8
#define clz_64 __builtin_clzll
#else
#error Unhandled type sizes!
#endif

template<typename T>
static _always_inline uint8_t bit_clz_(
        T n, integral_constant<size_t, 1>::type)
{
    return clz_32(n) - 24;
}

template<typename T>
static _always_inline uint8_t bit_clz_(
        T n, integral_constant<size_t, 2>::type)
{
    return clz_32(n) - 16;
}

template<typename T>
static _always_inline uint8_t bit_clz_(
        T n, integral_constant<size_t, 4>::type)
{
    return clz_32(n);
}

template<typename T>
static _always_inline uint8_t bit_clz_(
        T n, integral_constant<size_t, 8>::type)
{
    return clz_64(n);
}

template<typename T>
static _always_inline uint8_t bit_clz(T n)
{
    return bit_clz_(n, typename integral_constant<size_t, sizeof(T)>::type());
}

template<typename T>
static _always_inline uint8_t bit_log2_n(T n)
{
    uint8_t top = (sizeof(T)*8-1) - bit_clz(n);
    return top + !!(~-(T(1) << top) & n);
}
