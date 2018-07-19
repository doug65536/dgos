#pragma once
#include "types.h"

__BEGIN_NAMESPACE_STD

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

__END_NAMESPACE_STD
