#pragma once
#include "likely.h"
#include "type_traits.h"

__BEGIN_NAMESPACE_STD

template<typename _InputIt, typename _T>
_InputIt find(_InputIt __first, _InputIt __last, _T const& __v)
{
    while (__first != __last && *__first != __v)
        ++__first;
    return __first;
}

template<typename _InputIt, typename _UnaryPredicate>
_InputIt find_if(_InputIt __first, _InputIt __last, _UnaryPredicate&& __p)
{
    while (__first != __last && !__p(*__first))
        ++__first;
    return __first;
}

template<typename _InputIt, typename _UnaryPredicate>
_InputIt find_if_not(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last && __p(*__first))
        ++__first;
    return __first;
}

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
        new (&*__first) typename remove_reference<T>::type(__value);
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

template<typename _InputIt, typename _OutputIt>
_OutputIt copy(_InputIt __first, _InputIt __last, _OutputIt __out)
{
    for ( ; __first != __last; ++__first, ++__out)
        *__out = *__first;
    return __out;
}

template<typename _ForwardIt>
constexpr _ForwardIt min_element(_ForwardIt __first, _ForwardIt __last)
{
    if (__first != __last) {
        _ForwardIt __smallest = __first;

        for (++__first; __first != __last; ++__first) {
            if (*__first < *__smallest)
                __smallest = __first;
        }

        return __smallest;
    }

    return __last;
}

template<typename _ForwardIt>
constexpr _ForwardIt max_element(_ForwardIt __first, _ForwardIt __last)
{
    if (__first != __last) {
        _ForwardIt __largest = __first;

        for (++__first; __first != __last; ++__first) {
            if (*__largest < *__first)
                __largest = __first;
        }

        return __largest;
    }

    return __last;
}

template<typename _ForwardIt, typename _Compare>
constexpr _ForwardIt min_element(_ForwardIt __first, _ForwardIt __last,
                                 _Compare __cmp)
{
    if (__first != __last) {
        _ForwardIt __smallest = __first;

        for (++__first; __first != __last; ++__first) {
            if (__cmp(*__first, *__smallest) < 0)
                __smallest = __first;
        }

        return __smallest;
    }

    return __last;
}

template<typename _ForwardIt, typename _Compare>
constexpr _ForwardIt max_element(_ForwardIt __first, _ForwardIt __last,
                                 _Compare __cmp)
{
    if (__first != __last) {
        _ForwardIt __largest = __first;

        for (++__first; __first != __last; ++__first) {
            if (__cmp(*__largest, *__first) < 0)
                __largest = __first;
        }

        return __largest;
    }

    return __last;
}

__END_NAMESPACE_STD
