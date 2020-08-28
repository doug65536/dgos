#pragma once
#include "types.h"

__BEGIN_NAMESPACE_EXT

namespace detail {
    static constexpr intmax_t gcd(intmax_t a, intmax_t b)
    {
        intmax_t t{};
        while (b) {
            t = b;
            b = a % b;
            a = t;
        }
        return a;
    }
}

template<intmax_t Num, intmax_t Den = 1>
class ratio
{
public:
    using type = ratio<
        Num / detail::gcd(Num, Den),
        Den / detail::gcd(Num, Den)>;

    static constexpr intmax_t const num = Num;
    static constexpr intmax_t const den = Den;
};

//bit much using yocto = ratio<1, 1000000000000000000000000>;
//bit much using zepto = ratio<1, 1000000000000000000000>;
using atto  = ratio<1, 1000000000000000000>;
using femto = ratio<1, 1000000000000000>;
using pico  = ratio<1, 1000000000000>;
using nano  = ratio<1, 1000000000>;
using micro = ratio<1, 1000000>;
using milli = ratio<1, 1000>;
using centi = ratio<1, 100>;
using deci  = ratio<1, 10>;
using deca  = ratio<10, 1>;
using hecto = ratio<100, 1>;
using kilo  = ratio<1000, 1>;
using mega  = ratio<1000000, 1>;
using giga  = ratio<1000000000, 1>;
using tera  = ratio<1000000000000, 1>;
using peta  = ratio<1000000000000000, 1>;
using exa   = ratio<1000000000000000000, 1>;
//bit much using zetta = ratio<1000000000000000000000, 1>;
//bit much using yotta = ratio<1000000000000000000000000, 1>;

template<typename _R1, typename _R2>
using ratio_divide = typename ratio<
    intmax_t(_R1::num) * intmax_t(_R2::den),
    intmax_t(_R1::den) * intmax_t(_R2::num)
>::type;

template<typename _R1, typename _R2>
using ratio_add = typename ratio<
    ratio<
        _R1::num * _R2::den + _R2::num * _R1::den,
        _R1::den * _R2::den
    >::type::num,
    ratio<
        _R1::num * _R2::den + _R2::num * _R1::den,
        _R1::den * _R2::den
    >::type::den
>::type;

template<typename _R1, typename _R2>
using ratio_multiply = typename ratio<
    ratio<
        _R1::num * _R2::num,
        _R1::den * _R2::den
    >::type::num,
    ratio<
        _R1::num * _R2::num,
        _R1::den * _R2::den
    >::type::den
>::type;

template<typename _R1, typename _R2>
using ratio_subtract = typename ratio<
    ratio<
        _R1::num * _R2::den - _R2::num * _R1::den,
        _R1::den * _R2::den
    >::type::num,
    ratio<
        _R1::num * _R2::den - _R2::num * _R1::den,
        _R1::den * _R2::den
    >::type::den
>::type;



__END_NAMESPACE_STD
