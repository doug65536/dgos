#pragma once

#include "../likely.h"

#ifdef __DGOS_KERNEL__
#include "types.h"
#else
#include <stdint.h>
#endif

#include "initializer_list.h"
#include "utility.h"
#include "algorithm.h"
#ifdef __DGOS_KERNEL__
#include "printk.h"
#endif
#include "bitsearch.h"
#include "memory.h"

#if defined(_VECTOR_COMPLAIN_EN) && _VECTOR_COMPLAIN_EN
#define VECTOR_COMPLAIN(...) __VA_ARGS__
#else
#define _VECTOR_COMPLAIN(...)
#endif

__BEGIN_NAMESPACE_STD

template<typename _T, typename _Allocator = allocator<_T>>
class vector
{
public:
    using value_type = _T;
    using allocator_type = _Allocator;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = _T&;
    using const_reference = _T const&;
    using pointer = _T*;
    using const_pointer = _T const*;

    template<int _Dir, bool _Is_const>
    class __vector_iter;

    using iterator = __vector_iter<1, false>;
    using const_iterator = __vector_iter<1, true>;
    using reverse_iterator = __vector_iter<-1, false>;
    using const_reverse_iterator = __vector_iter<-1, true>;

    inline constexpr vector();

    constexpr explicit vector(_Allocator const& __alloc_);

    vector(size_type __count,
           _T const& __value,
           _Allocator const& __alloc_ = _Allocator());

    explicit vector(size_type __count,
                    _Allocator const& __alloc_ = _Allocator());

    template< typename InputIt >
    vector( InputIt __first, InputIt __last,
            _Allocator const& __alloc_ = _Allocator());

    vector(vector const& __other);
    vector(vector const& __other, _Allocator const& __alloc_);
    _always_inline constexpr vector(vector&& __other );
    inline constexpr vector(vector&& __other, _Allocator const& __alloc_);

    vector(initializer_list<_T> __init,
            _Allocator const& __alloc_ = _Allocator());

    ~vector();

    vector& operator=(vector const& __other);
    inline vector& operator=(vector&& __other);
    vector& operator=(initializer_list<_T> __ilist);

    bool assign(size_type __count, _T const& __value);

    template<typename _InputIt,
             typename _InputVal = decltype(*declval<_InputIt>())>
    bool assign(_InputIt __first, _InputIt __last);

    bool assign(initializer_list<_T> __ilist);

    allocator_type get_allocator() const;

    _T& at(size_type __pos);
    _T const& at(size_type __pos) const;

    _always_inline _T& operator[](size_type __pos);
    _always_inline _T const& operator[](size_type __pos) const;

    _always_inline _T& front();
    _always_inline _T const& front() const;

    _always_inline _T& back();
    _always_inline _T const& back() const;

    _always_inline pointer data();
    _always_inline const_pointer data() const;

    _always_inline iterator begin();
    _always_inline const_iterator begin() const;
    _always_inline const_iterator cbegin() const;

    _always_inline iterator end();
    _always_inline const_iterator end() const;
    _always_inline const_iterator cend() const;

    _always_inline reverse_iterator rbegin();
    _always_inline const_reverse_iterator rbegin() const;
    _always_inline const_reverse_iterator crbegin() const;

    _always_inline reverse_iterator rend();
    _always_inline const_reverse_iterator rend() const;
    _always_inline const_reverse_iterator crend() const;

    _always_inline bool empty() const;
    _always_inline size_type size() const;
    _always_inline size_type max_size() const;

    bool reserve(size_type __new_cap);
    inline size_type capacity() const;
    void shrink_to_fit();
    void clear();
    void reset();

    _VECTOR_COMPLAIN(_use_result)
    iterator insert(const_iterator __pos,
                    _T const& __value);

    _VECTOR_COMPLAIN(_use_result)
    iterator insert(const_iterator __pos,
                    _T&& __value);

    _VECTOR_COMPLAIN(_use_result)
    iterator insert(const_iterator __pos,
                    size_type __count,
                    _T const& __value);

    template<typename InputIt>
    _VECTOR_COMPLAIN(_use_result)
    iterator insert(const_iterator __pos,
                    InputIt __first, InputIt __last);

    _VECTOR_COMPLAIN(_use_result)
    iterator insert(const_iterator __pos,
                    initializer_list<_T> __ilist);

    template<typename... _Args >
    _VECTOR_COMPLAIN(_use_result)
    iterator emplace(const_iterator __pos,
                     _Args&&... __args);

    iterator erase(const_iterator __pos);

    iterator erase(const_iterator __first,
                   const_iterator __last);

    // Move the last element into the erased position with assignment
    iterator assign_erase(const_iterator __pos);

    // Move the last element into the erased position with move constructor
    iterator move_erase(const_iterator __pos);

    bool push_back(_T const& __value) noexcept;

    _VECTOR_COMPLAIN(_use_result)
    bool push_back(_T&& __value) noexcept;

    template<typename... _Args>
    _VECTOR_COMPLAIN(_use_result)
    bool emplace_back(_Args&&... __args) noexcept;

    void pop_back() noexcept;

    bool resize(size_type __count);

    bool resize(size_type __count,
                value_type const& __value);

    void swap(vector& __other);

    template<int _Dir, bool _Is_const>
    class __vector_iter_base
            : public contiguous_iterator_tag
    {
        using subclass =  typename conditional<_Is_const,
            const_reverse_iterator, reverse_iterator>::type;

        using base_result = typename conditional<_Is_const,
            const_iterator, iterator>::type;

    public:
        base_result base() const
        {
            subclass self = *static_cast<subclass const*>(this);
            return base_result(self.__p);
        }
    };

    template<bool _Is_const>
    class __vector_iter_base<1, _Is_const>
            : public contiguous_iterator_tag
    {
        // No base method on forward iterators
    };

    // Iterator
    template<int _Dir, bool _Is_const>
    class __vector_iter : public __vector_iter_base<_Dir, _Is_const>
    {
    private:
        friend class vector;
        friend class __vector_iter_base<_Dir, _Is_const>;

        _always_inline explicit constexpr __vector_iter(pointer __ip);

    public:
        _always_inline constexpr __vector_iter()
            : __p(nullptr)
        {
        }

        template<bool _RhsIsConst>
        _always_inline constexpr __vector_iter(
                __vector_iter<_Dir, _RhsIsConst> const& rhs);

        _always_inline constexpr _T& operator*();
        _always_inline _T const& operator*() const;
        _always_inline _T& operator[](size_type __n);
        _always_inline _T const& operator[](size_type __n) const;
        _always_inline constexpr pointer operator->();
        _always_inline constexpr const_pointer operator->() const;

        _always_inline bool operator==(__vector_iter const& rhs) const;
        _always_inline bool operator!=(__vector_iter const& rhs) const;
        _always_inline bool operator<(__vector_iter const& rhs) const;
        _always_inline bool operator<=(__vector_iter const& rhs) const;
        _always_inline bool operator>=(__vector_iter const& rhs) const;
        _always_inline bool operator>(__vector_iter const& rhs) const;

        _always_inline __vector_iter& operator++();
        _always_inline __vector_iter operator++(int);

        _always_inline __vector_iter& operator--();
        _always_inline __vector_iter operator--(int);

        _always_inline __vector_iter& operator+=(difference_type diff);
        _always_inline __vector_iter& operator-=(difference_type diff);

        _always_inline __vector_iter operator+(difference_type diff) const;
        _always_inline constexpr __vector_iter operator-(
                difference_type diff) const;

        _always_inline constexpr difference_type operator-(
                __vector_iter const& rhs) const;

    private:
        pointer __p;
    };

private:
    size_type __best_cap(size_t __proposed);
    bool __grow(size_t __amount = 1);
    pointer __make_space(iterator __pos, size_t __count);

    pointer __m;
    size_type __capacity;
    size_type __sz;
    _Allocator __alloc;
};

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::size_type
vector<_T,_Allocator>::__best_cap(size_t __amount)
{
    // Grow at least by the requested amount
    size_t __req_cap = __capacity + __amount;
    size_t __req_sz = __req_cap * sizeof(_T);

    // Next power of two
    size_t __best_sz = (size_t(1) << bit_log2(
                            __req_sz + _MALLOC_OVERHEAD)) - _MALLOC_OVERHEAD;

    // Round up to first size
    size_t __min_sz = 64 - _MALLOC_OVERHEAD;

    __best_sz = __best_sz >= __min_sz ? __best_sz : __min_sz;

    size_t __new_cap = __best_sz / sizeof(_T);

    return __new_cap;
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::__grow(size_t __amount)
{
    if (__sz + __amount > __capacity) {
        size_type __new_cap = __best_cap(__amount);
        return reserve(__new_cap);
    }
    return true;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::pointer
vector<_T,_Allocator>::__make_space(iterator __pos, size_t __count)
{
    if (__capacity < __sz + __count) {
        // Grow and fixup iterator pointer
        difference_type __pos_ofs =
                difference_type(__pos.__p) - difference_type(__m);
        if (!__grow(__sz + __count - __capacity))
            return nullptr;
        __pos.__p = pointer(difference_type(__m) + __pos_ofs);
    }

    if (__pos.__p == nullptr)
        __pos.__p = __m;

    size_t e = __pos.__p - __m;
    for (size_t __i = __sz; __i > e; --__i) {
        new (__m + __i + __count - 1) value_type(move(__m[__i - 1]));
        __m[__i - 1].~value_type();
    }
    return __pos.__p;
}

//
// vector implementation

template<typename _T, typename _Allocator>
constexpr vector<_T,_Allocator>::vector()
    : vector(_Allocator())
{
}

template<typename _T, typename _Allocator>
constexpr vector<_T,_Allocator>::vector(_Allocator const& __alloc_)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(__alloc_)
{
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(size_type __count,
       _T const& __value,
       _Allocator const& __alloc_)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(__alloc_)
{
    resize(__count, __value);
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(
        size_type __count, _Allocator const& __alloc_)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(__alloc_)
{
    if (unlikely(!reserve(__count)))
        panic_oom();
    // Default construct in-place, no copy
    ext::uninitialized_emplace<_T>(__m, __m + __count);
    __sz = __count;
}

template<typename _T, typename _Allocator>
template< typename InputIt >
vector<_T,_Allocator>::vector(InputIt __first, InputIt __last,
        _Allocator const& __alloc_)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(__alloc_)
{
    assign(__first, __last);
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(vector const& __other)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(_Allocator())
{
    if (unlikely(!reserve(__other.__sz)))
        panic_oom();
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(vector const& __other,
                              _Allocator const& __alloc_)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(__alloc_)
{
    if (unlikely(!reserve(__other.__sz)))
        panic_oom();
}

template<typename _T, typename _Allocator>
constexpr vector<_T,_Allocator>::vector(vector&& __other )
    : __m(__other.__m)
    , __capacity(__other.__capacity)
    , __sz(__other.__sz)
    , __alloc(move(__other.__alloc))
{
    __other.__m = nullptr;
    __other.__capacity = 0;
    __other.__sz = 0;
}

template<typename _T, typename _Allocator>
constexpr vector<_T,_Allocator>::vector(vector&& __other,
                              _Allocator const& __alloc_)
    : __m(__other.__m)
    , __capacity(__other.__capacity)
    , __sz(__other.__sz)
    , __alloc(__alloc_)
{
    __other.__m = nullptr;
    __other.__capacity = 0;
    __other.__sz = 0;
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(initializer_list<_T> __init,
        _Allocator const& __alloc_)
    : __m(nullptr)
    , __sz(0)
    , __alloc(__alloc_)
{
    assign(move(__init));
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::~vector()
{
    clear();
    if (__m) {
        __alloc.deallocate(__m, __capacity);
        __m = nullptr;
    }
    __capacity = 0;
    __sz = 0;
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>&
vector<_T,_Allocator>::operator=(vector const& other)
{
    assign(other.begin(), other.end());
    return *this;
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>&
vector<_T,_Allocator>::operator=(vector&& __other)
{
    if (*this != __other) {
        if (__m) {
            __alloc.deallocate(__m, __capacity);
            __m = nullptr;
        }


        __m = __other.__m;
        __sz = __other.__sz;
        __capacity = __other.__capacity;
        __alloc = move(__other.__alloc);
        __other.__m = nullptr;
        __other.__sz = 0;
        __other.__capacity = 0;
    }
    return *this;
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>&
vector<_T,_Allocator>::operator=(initializer_list<_T> __ilist)
{
    assign(move(__ilist));
    return *this;
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::assign(size_type __count, _T const& __value)
{
    clear();
    if (!reserve(__count))
        return false;
    uninitialized_fill(__m, __m + __count, __value);
    __sz = __count;
    return true;
}

template<typename _T, typename _Allocator>
template<typename _InputIt, typename _InputVal>
bool vector<_T,_Allocator>::assign(_InputIt __first, _InputIt __last)
{
    clear();
    if (!reserve(__last - __first))
        return false;
    for ( ; __first != __last; ++__first)
        new (__m + __sz++) value_type(*__first);
    return true;
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::assign(initializer_list<_T> __ilist)
{
    if (!reserve(__ilist.size()))
        return false;
    return assign(__ilist.begin(), __ilist.end());
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::allocator_type
vector<_T,_Allocator>::get_allocator() const
{
    return __alloc;
}

template<typename _T, typename _Allocator>
_T&
vector<_T,_Allocator>::at(size_type __pos)
{
    if (likely(__pos < __sz))
        return __m[__pos];
    panic("vector access out of range");
}

template<typename _T, typename _Allocator>
_T const&
vector<_T,_Allocator>::at(size_type __pos) const
{
    if (likely(__pos < __sz))
        return __m[__pos];
    panic("vector access out of range");
}

template<typename _T, typename _Allocator>
_T&
vector<_T,_Allocator>::operator[](size_type __pos)
{
    return __m[__pos];
}

template<typename _T, typename _Allocator>
_T const&
vector<_T,_Allocator>::operator[](size_type __pos) const
{
    return __m[__pos];
}

template<typename _T, typename _Allocator>
_T&
vector<_T,_Allocator>::front()
{
    assert(__sz > 0);
    return __m[0];
}

template<typename _T, typename _Allocator>
_T const&
vector<_T,_Allocator>::front() const
{
    assert(__sz > 0);
    return __m[0];
}

template<typename _T, typename _Allocator>
_T&
vector<_T,_Allocator>::back()
{
    return __m[__sz - 1];
}

template<typename _T, typename _Allocator>
_T const&
vector<_T,_Allocator>::back() const
{
    assert(__sz > 0);
    return __m[__sz - 1];
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::pointer
vector<_T,_Allocator>::data()
{
    return __m;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_pointer
vector<_T,_Allocator>::data() const
{
    return __m;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::begin()
{
    return iterator(__m);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_iterator
vector<_T,_Allocator>::begin() const
{
    return const_iterator(__m);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_iterator
vector<_T,_Allocator>::cbegin() const
{
    return const_iterator(__m);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::end()
{
    return iterator(__m + __sz);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_iterator
vector<_T,_Allocator>::end() const
{
    return const_iterator(__m + __sz);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_iterator
vector<_T,_Allocator>::cend() const
{
    return const_iterator(__m + __sz);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::reverse_iterator
vector<_T,_Allocator>::rbegin()
{
    return reverse_iterator(__m + __sz);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_reverse_iterator
vector<_T,_Allocator>::rbegin() const
{
    return const_reverse_iterator(__m + __sz);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_reverse_iterator
vector<_T,_Allocator>::crbegin() const
{
    return const_reverse_iterator(__m + __sz);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::reverse_iterator
vector<_T,_Allocator>::rend()
{
    return reverse_iterator(__m);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_reverse_iterator
vector<_T,_Allocator>::rend() const
{
    return const_reverse_iterator(__m);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::const_reverse_iterator
vector<_T,_Allocator>::crend() const
{
    return const_reverse_iterator(__m);
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::empty() const
{
    return __sz == 0;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::size_type
vector<_T,_Allocator>::size() const
{
    return __sz;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::size_type
vector<_T,_Allocator>::max_size() const
{
    return ~size_type(0) / sizeof(_T);
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::reserve(size_type __new_cap)
{
    if (__capacity < __new_cap) {
        unique_ptr<value_type> new_p = __alloc.allocate(__new_cap);
        if (unlikely(!new_p))
            return false;
        if (__sz)
            uninitialized_move(__m, __m + __sz, new_p.get());
        for (size_t __i = 0; __i < __sz; ++__i)
            __m[__i].~value_type();
        if (__m != nullptr)
            __alloc.deallocate(__m, __capacity);
        __m = new_p.release();
        __capacity = __new_cap;
    }
    return true;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::size_type
vector<_T,_Allocator>::capacity() const
{
    return __capacity;
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::shrink_to_fit()
{
    if (__capacity > __sz) {
        pointer __tmp = __alloc.allocate(__sz);
        uninitialized_move(__m, __m + __sz, __tmp);
        __alloc.deallocate(__m, __capacity);
        __m = __tmp;
        __capacity = __sz;
    }
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::clear()
{
    if (std::has_trivial_destructor<_T>::value) {
        __sz = 0;
    } else {
        while (__sz > 0)
            pop_back();
    }
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::reset()
{
    clear();
    if (__capacity > 0)
        __alloc.deallocate(__m, __capacity);
    __m = nullptr;
    __sz = 0;
    __capacity = 0;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos, _T const& __value)
{
    iterator __it(__pos.__p);

    constexpr size_t __count = 1;

    new (__make_space(__it, __count)) value_type(__value);

    __sz += __count;

    return __it;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos, _T&& __value)
{
    iterator __it(__pos.__p);

    constexpr size_t __count = 1;

    new (__make_space(__it, __count)) value_type(move(__value));

    __sz += __count;

    return __it;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(
        const_iterator __pos, size_type __count, _T const& __value)
{
    iterator __it = iterator(__pos.__p);
    pointer place = __make_space(__it, __count);
    if (likely(place)) {
        uninitialized_fill(place, place + __count, __value);

        __sz += __count;
    }

    return __it;
}

template<typename _T, typename _Allocator>
template<typename InputIt>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos,
                              InputIt __first, InputIt __last)
{
    iterator __it(__pos.__p);
    size_t __count = __last - __first;
    pointer place = __make_space(__it, __count);
    if (likely(place))
        uninitialized_copy(__first, __last, place);
    else
        __it.__p = nullptr;

    return __it;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos,
                              initializer_list<_T> __ilist)
{
    return iterator(insert(__pos, __ilist.begin(), __ilist.end()).__p);
}

template<typename _T, typename _Allocator>
template<typename... _Args >
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::emplace(const_iterator __pos, _Args&&... __args)
{
    pointer place = __make_space(__pos, 1);
    new (place) value_type(forward<_Args>(__args)...);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::erase(const_iterator __pos)
{
    constexpr size_t __count = 1;
    pointer __p = __pos.__p;
    pointer __e = __m + __sz - __count;
    for ( ; __p < __e; ++__p) {
        __alloc.destruct(__p);
        new (__p) value_type(move(__p[__count]));
    }
    __alloc.destruct(__e);
    --__sz;
    return iterator(__pos.__p);
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::erase(const_iterator __first, const_iterator __last)
{
    size_t __count = __last - __first;
    pointer __p = __first.__p;
    pointer __e = __m + __sz - __count;
    for ( ; __p < __e; ++__p) {
        __alloc.destruct(__p);
        new (__p) value_type(move(__p[__count]));
    }
    __alloc.destruct(__e);
    __sz -= __count;
    return iterator(__first.__p);
}

template<typename _T, typename _Allocator>
typename vector<_T, _Allocator>::iterator
vector<_T, _Allocator>::assign_erase(const_iterator __pos)
{
    *__pos.__p = move(__m[--__sz]);
    __m[__sz].~value_type();
    return iterator(__pos.__p);
}

template<typename _T, typename _Allocator>
typename vector<_T, _Allocator>::iterator
vector<_T, _Allocator>::move_erase(const_iterator __pos)
{
    __alloc.destruct(__pos.__p);
    new (__pos.__p) value_type(move(__m[--__sz]));
    __m[__sz].~value_type();
    return iterator(__pos.__p);
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::push_back(_T const& __value) noexcept
{
    if (unlikely(__sz + 1 > __capacity)) {
        if (unlikely(!__grow()))
            return false;
    }

    new (__m + __sz) value_type(__value);

    ++__sz;

    return true;
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::push_back(_T&& __value) noexcept
{
    if (unlikely(__sz + 1 > __capacity)) {
        if (unlikely(!__grow()))
            // Return with object intact and unmodified
            return false;
    }

    // Move begins when certain memory space exists
    new (__m + __sz) value_type(move(__value));

    ++__sz;

    return true;
}

template<typename _T, typename _Allocator>
template<typename... _Args>
bool vector<_T,_Allocator>::emplace_back(_Args&& ...__args) noexcept
{
    if (unlikely(__sz + 1 > __capacity)) {
        if (unlikely(!__grow()))
            return false;
    }

    new (__m + __sz) value_type(forward<_Args>(__args)...);

    ++__sz;

    return true;
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::pop_back() noexcept
{
    __alloc.destruct(__m + --__sz);
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::resize(size_type __count)
{
    return resize(__count, value_type());
}

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::resize(size_type __count,
                                   value_type const& __value)
{
    if (__sz > __count) {
        for (size_t __i = __sz; __i > __count; --__i)
            __alloc.destruct(__m + (__i - 1));
        __sz = __count;
    } else if (__sz < __count) {
        if (unlikely(!reserve(__count)))
            return false;
        uninitialized_fill(__m + __sz, __m + __count, __value);
        __sz = __count;
    }

    return true;
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::swap(vector &__other)
{
    std::swap(__m, __other.__m);
    std::swap(__sz, __other.__sz);
    std::swap(__capacity, __other.__capacity);
    std::swap(__alloc, __other.__alloc);
}

//
// Comparison

template< typename _R, typename _Alloc >
bool operator==(vector<_R,_Alloc> const& lhs,
                vector<_R,_Alloc> const& rhs)
{
    return equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template< typename _R, typename _Alloc >
bool operator!=(vector<_R,_Alloc> const& lhs,
                vector<_R,_Alloc> const& rhs)
{
    return !equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template< typename _R, typename _Alloc >
bool operator<(vector<_R,_Alloc> const& lhs,
               vector<_R,_Alloc> const& rhs);

template< typename _R, typename _Alloc >
bool operator<=(vector<_R,_Alloc> const& lhs,
                vector<_R,_Alloc> const& rhs);

template< typename _R, typename _Alloc >
bool operator>(vector<_R,_Alloc> const& lhs,
               vector<_R,_Alloc> const& rhs);

template< typename _R, typename _Alloc >
bool operator>=(vector<_R,_Alloc> const& lhs,
                vector<_R,_Alloc> const& rhs);

template<typename _T, typename _Alloc>
void swap(vector<_T,_Alloc>& lhs,
          vector<_T,_Alloc>& rhs);

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
template<bool _RhsIsConst>
constexpr vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::__vector_iter(
        __vector_iter<_Dir, _RhsIsConst> const& rhs)
    : __p(rhs.__p)
{
    // --------+---------+---------
    // LHS     | RHS     | Allowed
    // --------+---------+---------
    // mutable | mutable | yes
    // mutable | const   | no
    // const   | mutable | yes
    // const   | const   | yes
    // --------+---------+---------

    static_assert(_Is_const || !_RhsIsConst,
                  "Cannot copy const_(reverse_)iterator to (reverse_)iterator");
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
constexpr
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::__vector_iter(pointer __ip)
    : __p(__ip)
{
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
constexpr _T&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator *()
{
    return __p[-(_Dir < 0)];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T const&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator *() const
{
    return __p[-(_Dir < 0)];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator[](size_type __n)
{
    return __p[__n * _Dir - (_Dir < 0)];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T const&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator[](size_type __n) const
{
    return __p[__n * _Dir - (_Dir < 0)];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
constexpr typename vector<_T,_Alloc>::pointer
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator->()
{
    return &__p[-(_Dir < 0)];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
constexpr typename vector<_T,_Alloc>::const_pointer
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator->() const
{
    return &__p[-(_Dir < 0)];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator==(
        __vector_iter const& rhs) const
{
    return __p == rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator!=(
        __vector_iter const& rhs) const
{
    return __p != rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator<(
        __vector_iter const& rhs) const
{
    return __p < rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator<=(
        __vector_iter const& rhs) const
{
    return __p <= rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator>=(
        __vector_iter const& rhs) const
{
    return __p >= rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator>(
        __vector_iter const& rhs) const
{
    return __p > rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator++()
{
    __p += _Dir;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator++(int)
{
    return __vector_iter((__p += _Dir) - _Dir);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator--()
{
    __p -= _Dir;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator--(int)
{
    return __vector_iter((__p -= _Dir) + _Dir);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator+=(
        difference_type diff)
{
    __p += diff * _Dir;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator-=(
        difference_type diff)
{
    __p -= diff * _Dir;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator+(
        difference_type diff) const
{
    return __vector_iter(__p + diff * _Dir);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
constexpr typename vector<_T,_Alloc>::template __vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator-(
        difference_type diff) const
{
    return __vector_iter(__p - diff * _Dir);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
constexpr typename vector<_T,_Alloc>::difference_type
vector<_T,_Alloc>::__vector_iter<_Dir, _Is_const>::operator-(
        __vector_iter const& rhs) const
{
    return (__p - rhs.__p) * _Dir;
}

//
// begin/end overloads for vector

template<typename _T, typename _Alloc>
typename vector<_T, _Alloc>::iterator
begin(vector<_T, _Alloc>& __rhs)
{
    return __rhs.begin();
}

template<typename _T, typename _Alloc>
typename vector<_T, _Alloc>::const_iterator
begin(vector<_T, _Alloc> const& __rhs)
{
    return __rhs.begin();
}

template<typename _T, typename _Alloc>
typename vector<_T, _Alloc>::const_iterator
cbegin(vector<_T, _Alloc> const& __rhs)
{
    return __rhs.begin();
}

template<typename _T, typename _Alloc>
typename vector<_T, _Alloc>::iterator
end(vector<_T, _Alloc>& __rhs)
{
    return __rhs.end();
}

template<typename _T, typename _Alloc>
typename vector<_T, _Alloc>::const_iterator
end(vector<_T, _Alloc> const& __rhs)
{
    return __rhs.end();
}

template<typename _T, typename _Alloc>
typename vector<_T, _Alloc>::const_iterator
cend(vector<_T, _Alloc> const& __rhs)
{
    return __rhs.end();
}

__END_NAMESPACE_STD

extern template class std::vector<char, std::allocator<char>>;
extern template class std::vector<uint8_t, std::allocator<uint8_t>>;
extern template class std::vector<char16_t, std::allocator<char16_t>>;
extern template class std::vector<wchar_t, std::allocator<wchar_t>>;
