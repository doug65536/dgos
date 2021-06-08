
#include <sys/math_targets.h>

//              ∞       1        z - 1
// ln(z) = 2 *  ∑  * ------ * (( ----- ) ^ (2𝑛+1))
//             𝑛=0   2𝑛 + 1      z + 1
template<typename T>
inline __attribute__((__always_inline__))
constexpr T __logimpl(T z)
{
    T v = (z - T(1)) / (z + T(1));
    T v2 = v * v;
    T d = T(1);
    T t = 0;
    T p;

    do
    {
        p = t;
        t += v / d;
        d += T(2);
        v *= v2;
    } while (p != t);

    return t + t;
}

extern "C"
__TARGET_CLONES
double log(double z)
{
    return __logimpl<double>(z);
}

extern "C"
__TARGET_CLONES
float logf(float z)
{
    return __logimpl<float>(z);
}

extern "C"
long double logl(long double z)
{
    return __logimpl<long double>(z);
}
