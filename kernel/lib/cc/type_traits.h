#pragma once

template<bool _Enable, typename _T = void>
struct enable_if
{
};

template<typename _T>
struct enable_if<true, _T>
{
    typedef _T type;
};

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
    static constexpr _T val = _Val;

    operator value_type() const { return _Val; }
    value_type operator()() const { return _Val; }
};

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

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
