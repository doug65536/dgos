#pragma once
#include "types.h"

template<typename T>
struct default_delete
{
    constexpr default_delete() = default;

    inline void operator()(T* ptr) const
    {
        delete ptr;
    }
};

template<typename T>
struct default_delete<T[]>
{
    constexpr default_delete() = default;

    template<typename U>
    inline void operator()(U ptr) const
    {
        delete[] ptr;
    }
};

template<typename _T,
		 typename Tdeleter = default_delete<_T>>
class unique_ptr
{
public:
	using value_type = _T;
	using pointer = _T*;
	using const_pointer = _T const*;

    unique_ptr()
        : ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& rhs)
        : ptr(rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

	unique_ptr(_T* value)
        : ptr(value)
    {
    }

    ~unique_ptr()
    {
        if (ptr)
            ((Tdeleter()))(ptr);
    }

	operator _T*()
    {
        return ptr;
    }

	operator _T const*() const
    {
        return ptr;
    }

	_T* operator->()
    {
        return ptr;
    }

	_T const* operator->() const
    {
        return ptr;
    }

	_T* get()
    {
        return ptr;
    }

	_T const* get() const
    {
        return ptr;
    }

	unique_ptr &operator=(_T* rhs)
    {
        if (ptr)
            ((Tdeleter()))(ptr);
        ptr = rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

	_T* release()
    {
		_T* p = ptr;
        ptr = nullptr;
        return p;
    }

	void reset(_T* p = pointer())
    {
		_T* old = ptr;
        ptr = p;
        if (old)
            ((Tdeleter()))(old);
    }

private:
	_T* ptr;
};

template<typename _T, typename Tdeleter>
class unique_ptr<_T[], Tdeleter>
{
public:
	using value_type = _T;
	using pointer = _T*;
	using const_pointer = _T const*;

    unique_ptr()
        : ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& rhs)
        : ptr(rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

	unique_ptr(_T* value)
        : ptr(value)
    {
    }

    ~unique_ptr()
    {
        if (ptr)
            ((Tdeleter()))(ptr);
    }

	operator _T*()
    {
        return ptr;
    }

	operator _T const*() const
    {
        return ptr;
    }

	_T& operator[](size_t i)
	{
		return ptr[i];
	}

	_T const& operator[](size_t i) const
	{
		return ptr[i];
	}

	_T* operator->()
    {
        return ptr;
    }

	_T const* operator->() const
    {
        return ptr;
    }

	_T* get()
    {
        return ptr;
    }

	_T const* get() const
    {
        return ptr;
    }

	unique_ptr &operator=(_T* rhs)
    {
        if (ptr)
            ((Tdeleter()))(ptr);
        ptr = rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

	_T* release()
    {
		_T* p = ptr;
        ptr = nullptr;
        return p;
    }

	void reset(_T* p = pointer())
    {
		_T* old = ptr;
        ptr = p;
        if (old)
            ((Tdeleter()))(old);
    }

private:
	_T* ptr;
};

template<typename T>
class free_deleter
{
public:
    constexpr free_deleter() = default;

    inline void operator()(T* ptr) const
    {
        if (ptr) {
            destruct(ptr);
            free(ptr);
        }
    }

private:
    void destruct(void *) const
    {
    }

    template<typename U>
    void destruct(U *ptr) const
    {
        ptr->~U();
    }
};

template<typename T>
using unique_ptr_free = unique_ptr<T, free_deleter<T>>;
