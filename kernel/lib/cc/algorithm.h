#pragma once
#include "likely.h"

template<typename _InputIt, typename _UnaryPredicate>
bool all_of(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last) {
        if (!__p(*__first))
            return false;
        ++__first;
    }
    return true;
}

template<typename _InputIt, typename _UnaryPredicate>
bool any_of(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last) {
        if (__p(*__first))
            return true;
        ++__first;
    }
    return false;
}

template<typename _InputIt, typename _UnaryPredicate>
bool none_of(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last) {
        if (__p(*__first))
            return false;
        ++__first;
    }
    return true;
}

template<typename _InputIt1, typename _InputIt2>
bool equal(_InputIt1 __first1, _InputIt1 __last1, _InputIt2 __first2)
{
    while (__first1 != __last1) {
        if (*__first1 != *__first2)
            return false;
        ++__first1;
        ++__first2;
    }
    return true;
}

template<typename _InputIt1, typename _InputIt2, typename _BinaryPredicate>
bool equal(_InputIt1 __first1, _InputIt1 __last1,
           _InputIt2 __first2, _BinaryPredicate __p)
{
    while (__first1 != __last1) {
        if (!__p(*__first1, *__first2))
            return false;
        ++__first1;
        ++__first2;
    }
    return true;
}

template<typename _InputIt1, typename _InputIt2>
bool equal(_InputIt1 __first1, _InputIt1 __last1,
           _InputIt2 __first2, _InputIt2 __last2 )
{
    // FIXME: if both are random iterators, should compare distances first
    for(;;) {
        if (unlikely(__first1 == __last1 && __first2 == __last2))
            return true;
        if (unlikely(__first1 == __last1))
            return false;
        if (unlikely(__first2 == __last2))
            return false;
        if (unlikely(*__first1 != *__first2))
            return false;
        ++__first1;
        ++__first2;
    }
}

template<typename _InputIt1, typename _InputIt2, typename _BinaryPredicate>
bool equal(_InputIt1 __first1, _InputIt1 __last1,
           _InputIt2 __first2, _InputIt2 __last2,
           _BinaryPredicate __p )
{
    // FIXME: if both are random iterators, should compare distances first
    for(;;) {
        if (unlikely(__first1 == __last1 && __first2 == __last2))
            return true;
        if (unlikely(__first1 == __last1))
            return false;
        if (unlikely(__first2 == __last2))
            return false;
        if (unlikely(!__p(*__first1, *__first2)))
            return false;
        ++__first1;
        ++__first2;
    }
}

template<typename _OutputIt, typename _Size, typename _T>
_OutputIt fill_n(_OutputIt __first, _Size __count, _T const& __value)
{
    while (__count > 0) {
        *__first = __value;
        ++__first;
        --__count;
    }
    return __first;
}

template<typename _InputIt, typename _OutputIt>
_OutputIt uninitialized_copy(_InputIt __first, _InputIt __last,
                             _OutputIt __out)
{
    using T = decltype(*__out);
    while (__first != __last) {
        new (&*__out) T(*__first);
        ++__out;
        ++__first;
    }
    return __out;
}

template<typename _OutputIt, typename _T>
_OutputIt uninitialized_fill(_OutputIt __first, _OutputIt __last,
                             _T const& __value)
{
    using T = decltype(*__first);
    while (__first != __last) {
        new (&*__first) T(__value);
        ++__first;
    }
    return __first;
}

template<typename _InputIt, typename _OutputIt>
_OutputIt uninitialized_move(_InputIt __first, _InputIt __last,
                             _OutputIt __out)
{
    using T = typename remove_reference<decltype(*__out)>::type;
    while (__first != __last) {
        new (&*__out) T(move(*__first));
        ++__out;
        ++__first;
    }
    return __out;
}
