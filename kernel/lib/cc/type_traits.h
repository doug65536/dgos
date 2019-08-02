#pragma once

#ifdef __DGOS_KERNEL__
#include "types.h"
#else
#include <stdint.h>
#endif

__BEGIN_NAMESPACE_STD

template<bool _Condition, typename _T, typename _F>
struct conditional
{
};

template<typename _T, typename _F>
struct conditional<true, _T, _F>
{
    using type = _T;
};

template<typename _T, typename _F>
struct conditional<false, _T, _F>
{
    using type = _F;
};

template<typename _T, _T _Val>
struct integral_constant
{
    using value_type = _T;
    using type = integral_constant<_T, _Val>;

    static constexpr _T value = _Val;

    operator value_type() const { return _Val; }
    value_type operator()() const { return _Val; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template<bool _Val>
struct bool_constant;

template<>
struct bool_constant<false> : public false_type
{
};

template<>
struct bool_constant<true> : public true_type
{
};

template<typename _T>
using has_nothrow_assign = bool_constant<__has_nothrow_assign(_T)>;

template<typename _T>
using has_nothrow_copy = bool_constant<__has_nothrow_copy(_T)>;

template<typename _T>
using has_nothrow_constructor = bool_constant<__has_nothrow_constructor(_T)>;

template<typename _T>
using has_trivial_assign = bool_constant<__has_trivial_assign(_T)>;

template<typename _T>
using has_trivial_copy = bool_constant<__has_trivial_copy(_T)>;

template<typename _T>
using has_trivial_constructor = bool_constant<__has_trivial_constructor(_T)>;

template<typename _T>
using has_trivial_destructor = bool_constant<__has_trivial_destructor(_T)>;

template<typename _T>
using has_virtual_destructor = bool_constant<__has_virtual_destructor(_T)>;

template<typename _T>
using is_abstract = bool_constant<__is_abstract(_T)>;

template<typename _Base, typename _Derived>
using is_base_of = bool_constant<__is_base_of(_Base, _Derived)>;

template<typename _T>
using is_class = bool_constant<__is_class(_T)>;

template<typename _T>
using is_empty = bool_constant<__is_empty(_T)>;

template<typename _T>
using is_enum = bool_constant<__is_enum(_T)>;

template<typename _T>
using is_literal_type = bool_constant<__is_literal_type(_T)>;

template<typename _T>
using is_pod = bool_constant<__is_pod(_T)>;

template<typename _T>
using is_polymorphic = bool_constant<__is_polymorphic(_T)>;

template<typename _T>
using is_standard_layout = bool_constant<__is_standard_layout(_T)>;

template<typename _T1, typename _T2>
struct is_same : public false_type
{
};

template<typename _T1>
struct is_same<_T1, _T1> : public true_type
{
};

template<typename _T>
using is_trivial = bool_constant<__is_trivial(_T)>;

template<typename _T>
using is_union = bool_constant<__is_union(_T)>;

template<typename _T>
struct is_lvalue_reference;

template<typename _T>
struct is_lvalue_reference<_T&> : public true_type
{
};

template<typename _T>
struct is_lvalue_reference<_T&&> : public false_type
{
};

template<typename _T>
struct is_signed : public true_type
{
};

template<>
struct is_signed<unsigned char> : public false_type
{
};

template<>
struct is_signed<unsigned short> : public false_type
{
};

template<>
struct is_signed<unsigned int> : public false_type
{
};

template<>
struct is_signed<unsigned long> : public false_type
{
};

template<>
struct is_signed<__uint128_t> : public false_type
{
};

template<typename _T>
struct is_unsigned
        : public conditional<
            is_signed<_T>::value, false_type, true_type
        >::type
{
};

template<typename T>
struct remove_const
{
    using type = T;
};

template<typename T>
struct remove_const<const T> {
    using type = T;
};

template<typename T>
struct remove_volatile
{
    using type = T;
};
template<typename T>
struct remove_volatile<volatile T>
{
    using type = T;
};

template< class T >
struct remove_cv {
    typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

template<typename _T>
struct __is_member_pointer_helper
        : public false_type
{
};

template<typename _T, typename _U >
struct __is_member_pointer_helper<_T _U::*>
        : public true_type
{
};

template< class _T >
struct is_member_pointer :
    public __is_member_pointer_helper<typename remove_cv<_T>::type>
{
};

template<typename _T>
struct underlying_type
{
    using type = __underlying_type(_T);
};

template<bool _Enable, typename _T = void>
struct enable_if
{
};

template<typename _T>
struct enable_if<true, _T>
{
    using type = _T;
};

template<typename _T>
struct remove_reference
{
    using type = _T;
};

template<typename _T >
struct remove_reference<_T&>
{
    using type = _T;
};

template<typename _T>
struct remove_reference<_T&&>
{
    using type = _T;
};

__END_NAMESPACE_STD
__BEGIN_NAMESPACE_EXT

template<int size, bool uns = true> struct type_from_size { };
template<> struct type_from_size<1, true> { using type = uint8_t; };
template<> struct type_from_size<2, true> { using type = uint16_t; };
template<> struct type_from_size<4, true> { using type = uint32_t; };
template<> struct type_from_size<8, true> { using type = uint64_t; };
template<> struct type_from_size<1, false> { using type = int8_t; };
template<> struct type_from_size<2, false> { using type = int16_t; };
template<> struct type_from_size<4, false> { using type = int32_t; };
template<> struct type_from_size<8, false> { using type = int64_t; };

// Helper that gives underlying type for enums and is not an error otherwise
template<typename _T>
class safe_underlying_type
{
private:
    template<typename, typename>
    struct helper;

    template<typename _U>
    struct helper<_U, std::true_type>
    {
        using type = typename std::underlying_type<_U>::type;
    };

    template<typename _U>
    struct helper<_U, std::false_type>
    {
        using type = _U;
    };

public:
    using type = typename helper<_T, typename std::is_enum<_T>::type>::type;
};

__END_NAMESPACE_EXT

__BEGIN_NAMESPACE_STD

template<size_t _Len, size_t _Align = alignof(max_align_t)>
struct aligned_storage {
    struct type {
        alignas(_Align) unsigned char data[_Len];
    };
};


__BEGIN_NAMESPACE_DETAIL

template<typename T>
struct type_identity {
    using type = T;
};

template<typename T>
auto try_add_lvalue_reference(int) -> type_identity<T&>;

template<typename T>
auto try_add_lvalue_reference(...) -> type_identity<T>;

template<typename T>
auto try_add_rvalue_reference(int) -> type_identity<T&&>;

template<typename T>
auto try_add_rvalue_reference(...) -> type_identity<T>;

__END_NAMESPACE // detail

template<typename T>
struct add_lvalue_reference : decltype(detail::try_add_lvalue_reference<T>(0)) {};

template<typename T>
struct add_rvalue_reference : decltype(detail::try_add_rvalue_reference<T>(0)) {};


template<typename T>
typename add_rvalue_reference<T>::type declval() noexcept;

__BEGIN_NAMESPACE_DETAIL

template<typename _T1, typename _T2, typename ..._Rest>
auto select_common(_T1&&, _T2&&, _Rest&& ...rest) ->
decltype(select_common(sum_result(declval<_T1>(), declval<_T2>()),
                    declval<_Rest>()...));

template<typename _T1, typename _T2>
auto select_common(_T1&&, _T2&&) -> decltype(declval<_T1>() + declval<_T2>());

template<typename _T1, typename _T2>
auto select_common(_T1&&, _T2&&) -> decltype(declval<_T1>() + declval<_T2>());

__END_NAMESPACE

template<typename... _Types>
class common_type
{
public:
    using type = decltype(detail::select_common(declval<_Types>()...));
};

template<typename T>
struct is_integral : public false_type {};

template<> struct is_integral<signed char> : public true_type {};
template<> struct is_integral<int> : public true_type {};
template<> struct is_integral<short> : public true_type {};
template<> struct is_integral<long> : public true_type {};
template<> struct is_integral<long long> : public true_type {};

template<> struct is_integral<unsigned char> : public true_type {};
template<> struct is_integral<unsigned short> : public true_type {};
template<> struct is_integral<unsigned int> : public true_type {};
template<> struct is_integral<unsigned long> : public true_type {};
template<> struct is_integral<unsigned long long> : public true_type {};

template<typename T>
struct remove_cvref {
    using type = typename remove_cv<typename remove_reference<T>::type>::type;
};


__END_NAMESPACE_STD
