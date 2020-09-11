#include <stdint.h>
#include <string.h>

static constexpr double PI = 3.14159265358979323;
static constexpr double PI2 = 6.28318530717958647;

namespace {

template<typename T>
struct float_traits
{
};

template<>
struct float_traits<float>
{
    using integral_type = int32_t;
    static constexpr uint32_t mantissa_bits = 23;
    static constexpr uint32_t exponent_mask = 0xFF;
    static constexpr uint32_t min_int_exp = 0x7F;
    static constexpr uint32_t max_int_exp =
            min_int_exp + mantissa_bits;
    static constexpr bool hidden_bit = true;
};

template<>
struct float_traits<double>
{
    using integral_type = int64_t;
    static constexpr uint32_t mantissa_bits = 51;
    static constexpr uint32_t exponent_mask = 0x7FF;
    static constexpr uint32_t min_int_exp = 0x3FF;
    static constexpr uint32_t max_int_exp =
            min_int_exp + mantissa_bits;
    static constexpr bool hidden_bit = true;
};

template<>
struct float_traits<long double>
{
    using integral_type = int64_t;
    static constexpr uint32_t mantissa_bits = 64;
    static constexpr uint32_t exponent_mask = 0x7FFF;
    static constexpr uint32_t min_int_exp = 0x3FFF;
    static constexpr uint32_t max_int_exp =
            min_int_exp + mantissa_bits;
    static constexpr bool hidden_bit = false;
};

template<typename T,
         typename I = typename float_traits<T>::integral_type,
         uint32_t mantissa_bits = float_traits<T>::mantissa_bits,
         uint32_t exp_mask = float_traits<T>::exponent_mask,
         uint32_t min_int_exp = float_traits<T>::min_int_exp,
         uint32_t max_int_exp = float_traits<T>::max_int_exp,
         bool hidden_bit = float_traits<T>::hidden_bit>
static inline float abs_floor_impl(T x)
{
    I n;

    if (x < 0)
        x = -x;

    memcpy(&n, &x, sizeof(n));

    uint32_t exponent;

    // If the mantissa is less than whole thing, then extract exponent normally
    // else handle long double 15 bit exponent
    if (mantissa_bits < sizeof(I) * 8) {
        exponent = uint32_t((n >> mantissa_bits) & exp_mask);
    } else {
        uint16_t exp16 = 0;
        memcpy(&exp16, (char*)&x + sizeof(I), sizeof(exp16));
        exponent = exp16;
    }

    if (exponent < min_int_exp)
        return 0.0f;

    if (exponent < max_int_exp) {
        uint32_t keep = exponent - min_int_exp;

        I mantissa_mask = ~I(0);

        if (mantissa_bits < sizeof(I) * 8)
            mantissa_mask = ~-(I(1) << (mantissa_bits - hidden_bit));

        // Extract mantissa bits only
        I fm = n & mantissa_mask;

        // Mask off bits that would have place values smaller than 1
        fm &= -(I(1) << (mantissa_bits - keep));

        // Replace only the mantissa
        n = (n & ~mantissa_mask) | fm;

        memcpy(&x, &n, sizeof(n));
    }

    return x;
}

template<typename T>
T abs_floor(T x)
{
    return abs_floor_impl<T>(x);
}

}//namespace

//              𝑥^3   𝑥^5          ∞  (-1)^𝑛 * 𝑥^(2*(𝑛+1))
// sin(𝑥) = 𝑥 - --- + --- - ... =  ∑  --------------------
//               3!    5!         𝑛=0      (2𝑛 + 1)!
template<typename T>
constexpr inline __attribute__((__always_inline__))
T __sinimpl(T x)
{
    if (x < T(-PI2))
        x /= T(PI2) * abs_floor<T>(-x / T(PI2));
    else if (x > T(PI2))
        x /= T(PI2) * abs_floor<T>(x / T(PI2));

    // Denominator, running factorial of multiples of two
    T d = 1;

    // Incremental factorial multipliers
    T f1 = 2;
    T f2 = 3;

    // Incremental powers of -1
    T s = -1;

    // First term is ((-1^0 * x^1) / 1!), which is x
    T t = x;

    // First iteration needs x cubed
    x *= x;

    // Incremental exponents of x^(n*2)
    T x2 = x;

    // Cubed
    x *= t;

    for (int n = 1; n < 20; ++n) {
        d *= f1;
        f1 += T(2.0);
        d *= f2;
        f2 += T(2.0);
        t += s * x / d;
        s = -s;
        x *= x2;
    }
    return t;
}

extern "C"
__attribute__((__target_clones__("default,avx")))
__attribute__((__flatten__))
double sin(double x)
{
    return __sinimpl<double>(x);
}

extern "C"
__attribute__((__target_clones__("default,avx")))
__attribute__((__flatten__))
float sinf(float x)
{
    return __sinimpl<float>(x);
}

extern "C"
long double sinl(long double x)
{
    return __sinimpl<long double>(x);
}
