#pragma once
#include "types.h"

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
template<typename T>
class safe_underlying_type
{
private:
    template<typename, typename>
    struct helper;

    template<typename U>
    struct helper<U, true_type>
    {
        using type = typename underlying_type<U>::type;
    };

    template<typename U>
    struct helper<U, false_type>
    {
        using type = U;
    };

public:
    using type = typename helper<T, typename is_enum<T>::type>::type;
};
