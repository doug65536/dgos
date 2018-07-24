
static constexpr double PI = 3.14159265358979323;
static constexpr double PI2 = 6.28318530717958647;

//              𝑥^3   𝑥^5          ∞  (-1)^𝑛 * x^(2*(𝑛+1))
// sin(𝑥) = 𝑥 - --- + --- - ... =  ∑  --------------------
//               3!    5!         𝑛=0      (2𝑛 + 1)!
template<typename T>
constexpr inline __attribute__((__always_inline__))
T __sininpl(T x)
{
    // Argument reduction to bring into -2pi to 2pi range
    while (x > T(PI2))
        x -= T(PI2);
    while (x < T(-PI2))
        x += T(PI2);

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
double sin(double x)
{
    return __sininpl<double>(x);
}

extern "C"
__attribute__((__target_clones__("default,avx")))
float sinf(float x)
{
    return __sininpl<float>(x);
}

extern "C"
long double sinl(long double x)
{
    return __sininpl<long double>(x);
}
