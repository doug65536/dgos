#pragma once
#include "types.h"

template<bool _Condition, typename _T, typename _F>
struct conditional
{
};

template<typename _T, typename _F>
struct conditional<true, _T, _F>
{
    typedef _T type;
};

template<typename _T, typename _F>
struct conditional<false, _T, _F>
{
    typedef _F type;
};

template<typename _T, _T _Val>
struct integral_constant
{
    typedef _T value_type;
    typedef integral_constant<_T, _Val> type;
    static constexpr _T value = _Val;

    operator value_type() const { return _Val; }
    value_type operator()() const { return _Val; }
};

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

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
struct is_lvalue_reference : public false_type
{
};

template<typename _T>
struct is_lvalue_reference<_T&&> : public true_type
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
    typedef _T type;
};

template<typename _T>
struct remove_reference
{
	typedef _T type;
};

template<typename _T >
struct remove_reference<_T&>
{
	typedef _T type;
};

template<typename _T>
struct remove_reference<_T&&>
{
	typedef _T type;
};

template<int size, bool uns = true> struct type_from_size { };
template<> struct type_from_size<1, true> { typedef uint8_t type; };
template<> struct type_from_size<2, true> { typedef uint16_t type; };
template<> struct type_from_size<4, true> { typedef uint32_t type; };
template<> struct type_from_size<8, true> { typedef uint64_t type; };
template<> struct type_from_size<1, false> { typedef int8_t type; };
template<> struct type_from_size<2, false> { typedef int16_t type; };
template<> struct type_from_size<4, false> { typedef int32_t type; };
template<> struct type_from_size<8, false> { typedef int64_t type; };
