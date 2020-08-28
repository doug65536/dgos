#pragma once
#include "types.h"
#include "type_traits.h"

__BEGIN_NAMESPACE_EXT

struct input_iterator_tag { };
struct output_iterator_tag { };
struct forward_iterator_tag : public input_iterator_tag { };
struct bidirectional_iterator_tag : public forward_iterator_tag { };
struct random_access_iterator_tag : public bidirectional_iterator_tag { };

// C++20
struct contiguous_iterator_tag: public random_access_iterator_tag { };

template<typename _T, size_t _N>
_T *begin(_T(&__array)[_N]) noexcept {
    return __array;
}

template<typename _T, size_t _N>
_T *end(_T(&__array)[_N]) noexcept {
    return __array + _N;
}

template<typename _T, size_t _N>
_T *begin(_T const (&__array)[_N]) noexcept {
    return __array;
}

template<typename _T, size_t _N>
_T *end(_T const (&__array)[_N]) noexcept {
    return __array + _N;
}

__BEGIN_NAMESPACE_DETAIL


template<typename _T, typename _U>
ptrdiff_t detect_difference_type(_U);

template<typename _T, typename _D = typename _T::difference_type>
_D detect_difference_type(int);


template<typename _T, typename _U>
typename ext::remove_reference<decltype(*ext::declval<_T>())>::type
detect_value_type(_U);

template<typename _T, typename _D = typename _T::value_type>
_D detect_value_type(int);


template<typename _T, typename _U>
typename ext::remove_reference<decltype(&*ext::declval<_T>())>::type
detect_pointer_type(_U);

template<typename _T, typename _D = typename _T::pointer>
_D detect_pointer_type(int);


template<typename _T, typename _U>
decltype(*ext::declval<_T>())
detect_reference_type(_U);

template<typename _T, typename _D = typename _T::reference>
_D detect_reference_type(int);

__END_NAMESPACE

template<typename _T>
class iterator_traits {
public:
    using difference_type = decltype(detail::detect_difference_type<_T>(0));
    using value_type = decltype(detail::detect_value_type<_T>(0));
    using pointer = decltype(detail::detect_pointer_type<_T>(0));
    using reference = decltype(detail::detect_reference_type<_T>(0));
    using iterator_category = random_access_iterator_tag;
};

template<typename _T>
class iterator_traits<_T*> {
public:
    using difference_type = ptrdiff_t;
    using value_type = _T;
    using pointer = _T*;
    using reference = _T&;
    using iterator_category = random_access_iterator_tag;
};

template<typename _T>
class iterator_traits<_T const*> {
public:
    using difference_type = ptrdiff_t;
    using value_type = _T;
    using pointer = _T const*;
    using reference = _T const&;
    using iterator_category = random_access_iterator_tag;
};

__END_NAMESPACE_EXT
