#include <sys/math_targets.h>

static constexpr double PI = 3.14159265358979323;
static constexpr double PI2 = 6.28318530717958647;

//              𝑥^2   𝑥^4          ∞  (-1)^𝑛 * 𝑥^(2*𝑛)
// cos(𝑥) = 1 - --- + --- - ... =  ∑  ----------------
//               2!    4!         𝑛=0      (2𝑛)!
template<typename T>
constexpr inline __attribute__((__always_inline__))
T __cosimpl(T x)
{
    // Argument reduction to bring into -2pi to 2pi range
    while (x > T(PI2))
        x -= T(PI2);
    while (x < T(-PI2))
        x += T(PI2);

    // Denominator, running factorial of multiples of two
    T d = 1;

    // Incremental factorial multipliers
    T f1 = 1;
    T f2 = 2;

    // Incremental powers of -1
    T s = -1;

    // First term is ((-1^0 * x^0) / 0!), which is 1
    T t = 1;

    // First iteration needs x squared
    x *= x;

    // Incremental exponents of x
    T x2 = x;

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
__TARGET_CLONES
double cos(double x)
{
    return __cosimpl<double>(x);
}

extern "C"
__TARGET_CLONES
float cosf(float x)
{
    return __cosimpl<float>(x);
}

extern "C"
long double cosl(long double x)
{
    return __cosimpl<long double>(x);
}
