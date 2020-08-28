#pragma once
#include "types.h"
#include "type_traits.h"

__BEGIN_NAMESPACE_EXT

template<typename _InputIt, typename _T>
_T accumulate(_InputIt __first, _InputIt __last, _T __init)
{
    for ( ; __first != __last; ++__first)
        __init = __init + *__first;
    return __init;
}

template<typename _InputIt, typename _T, typename _BinaryOperation>
_T accumulate(_InputIt __first, _InputIt __last, _T __init,
              _BinaryOperation __op)
{
    for ( ; __first != __last; ++__first)
        __init = __op(__init, *__first);
    return __init;
}

template<typename _T1, typename _T2>
constexpr typename common_type<_T1, _T2>::type gcd(_T1 a, _T2 b)
{
    typename common_type<_T1, _T2>::type t{};
    while (b) {
        t = b;
        b = a % b;
        a = t;
    }
    return a;
}

template<typename _T1, typename _T2>
constexpr typename common_type<_T1, _T2>::type lcm(_T1 a, _T2 b)
{
    return a / gcd(a, b) * b;
}

__END_NAMESPACE_STD
