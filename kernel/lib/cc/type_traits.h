#pragma once

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
