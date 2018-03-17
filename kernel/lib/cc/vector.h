#pragma once
#include "types.h"
#include "likely.h"
#include "initializer_list.h"
#include "utility.h"
#include "algorithm.h"
#include "printk.h"
#include "bitsearch.h"
#include "unique_ptr.h"
#include "memory.h"

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
    class vector_iter;

    using iterator = vector_iter<1, false>;
    using const_iterator = vector_iter<1, true>;
    using reverse_iterator = vector_iter<-1, false>;
    using const_reverse_iterator = vector_iter<-1, true>;

    vector();

    explicit vector(_Allocator const& __alloc_);

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
    vector(vector&& __other );
    vector(vector&& __other, _Allocator const& __alloc_);

    vector(initializer_list<_T> __init,
            const _Allocator& __alloc_ = _Allocator());

    ~vector();

    vector& operator=(vector const& __other);
    vector& operator=(vector&& __other);
    vector& operator=(initializer_list<_T> __ilist);

    void assign(size_type __count, _T const& __value);

    template<typename _InputIt>
    void assign(_InputIt __first, _InputIt __last);

    void assign(initializer_list<_T> __ilist);

    allocator_type get_allocator() const;

    _T& at(size_type __pos);
    _T const& at(size_type __pos) const;

    _T& operator[](size_type __pos);
    _T const& operator[](size_type __pos) const;

    _T& front();
    _T const& front() const;

    _T& back();
    _T const& back() const;

    pointer data();
    const_pointer data() const;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

    reverse_iterator rbegin();
    const_reverse_iterator rbegin() const;
    const_reverse_iterator crbegin() const;

    reverse_iterator rend();
    const_reverse_iterator rend() const;
    const_reverse_iterator crend() const;

    bool empty() const;
    size_type size() const;
    size_type max_size() const;

    bool reserve(size_type __new_cap);
    size_type capacity() const;
    void shrink_to_fit();
    void clear();

    iterator insert(const_iterator __pos,
                    _T const& __value);

    iterator insert(const_iterator __pos,
                    _T&& __value);

    iterator insert(const_iterator __pos,
                    size_type __count,
                    _T const& __value);

    template<typename InputIt>
    iterator insert(const_iterator __pos,
                    InputIt __first, InputIt __last);

    iterator insert(const_iterator __pos,
                    initializer_list<_T> __ilist);

    template<typename... _Args >
    iterator emplace(const_iterator __pos,
                     _Args&&... __args);

    iterator erase(const_iterator __pos);

    iterator erase(const_iterator __first,
                   const_iterator __last);

    // Move the last element into the erased position with assignment
    iterator assign_erase(const_iterator __pos);

    // Move the last element into the erased position with move constructor
    iterator move_erase(const_iterator __pos);

    bool push_back(_T const& __value);
    bool push_back(_T&& __value);

    template<typename... _Args>
    bool emplace_back(_Args&&... __args);

    void pop_back();

    bool resize(size_type __count);

    bool resize(size_type __count,
                value_type const& __value);

    void swap(vector& __other);

    // Iterator
    template<int _Dir, bool _Is_const>
    class vector_iter
    {
    private:
        friend class vector;
        __always_inline vector_iter(pointer __ip);

    public:
        __always_inline vector_iter();

        template<bool _RhsIsConst>
        __always_inline vector_iter(vector_iter<_Dir, _RhsIsConst> const& rhs);

        __always_inline _T& operator*();
        __always_inline _T const& operator*() const;
        __always_inline vector_iter operator+(size_type __n) const;
        __always_inline vector_iter operator-(size_type __n) const;
        __always_inline _T& operator[](size_type __n);
        __always_inline _T const& operator[](size_type __n) const;
        __always_inline pointer operator->();
        __always_inline const_pointer operator->() const;

        __always_inline bool operator==(vector_iter const& rhs) const;
        __always_inline bool operator!=(vector_iter const& rhs) const;
        __always_inline bool operator<(vector_iter const& rhs) const;
        __always_inline bool operator<=(vector_iter const& rhs) const;
        __always_inline bool operator>=(vector_iter const& rhs) const;
        __always_inline bool operator>(vector_iter const& rhs) const;

        __always_inline vector_iter& operator++();
        __always_inline vector_iter operator++(int);

        __always_inline vector_iter& operator--();
        __always_inline vector_iter operator--(int);

        __always_inline vector_iter& operator+=(difference_type diff);
        __always_inline vector_iter& operator-=(difference_type diff);

        __always_inline vector_iter operator+(difference_type diff) const;
        __always_inline vector_iter operator-(difference_type diff) const;

        __always_inline difference_type operator-(vector_iter const& rhs) const;

    private:
        pointer __p;
    };

private:
    bool __grow(size_t __amount = 1);
    pointer __make_space(iterator __pos, size_t __count);

    pointer __m;
    size_type __capacity;
    size_type __sz;
    _Allocator __alloc;
};

template<typename _T, typename _Allocator>
bool vector<_T,_Allocator>::__grow(size_t __amount)
{
    size_t __new_cap;

    if (__capacity) {
        __new_cap = size_t(1) << (bit_log2(__capacity + __amount));
    } else if (__capacity == 0 && __amount < 12) {
        __new_cap = 12;
    } else {
        __new_cap = __amount;
    }

    return reserve(__new_cap);
}


template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::pointer
vector<_T,_Allocator>::__make_space(iterator __pos, size_t __count)
{
    if (__capacity < __sz + __count)
        if (!__grow(__sz + __count - __capacity))
            return nullptr;

    if (__pos.__p == nullptr)
        __pos.__p = __m;

    size_t e = __pos.__p - __m;
    for (size_t i = __sz; i > e; --i) {
        new (__m + i + __count - 1) value_type(move(__m[i - 1]));
        __m[i - 1].~value_type();
    }
    return __pos.__p;
}

//
// vector implementation

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector()
    : vector(_Allocator())
{
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(_Allocator const& __alloc_)
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
    resize(__count);
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
    reserve(__other.size());
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(vector const& __other,
                              _Allocator const& __alloc_)
    : __m(nullptr)
    , __capacity(0)
    , __sz(0)
    , __alloc(__alloc_)
{
    reserve(__other.size());
}

template<typename _T, typename _Allocator>
vector<_T,_Allocator>::vector(vector&& __other )
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
vector<_T,_Allocator>::vector(vector&& __other,
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
        const _Allocator& __alloc_)
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
vector<_T,_Allocator>::operator=(vector&& other)
{
    if (*this != other) {
        if (__m) {
            __alloc.deallocate(__m);
            __m = nullptr;
        }
        __sz = 0;

        __m = other.__m;
        __sz = other.__sz;
        __alloc = move(other.__alloc);
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
void vector<_T,_Allocator>::assign(size_type __count, _T const& value)
{
    resize(0);
    while (__sz < __count)
        push_back(value);
}

template<typename _T, typename _Allocator>
template< typename InputIt >
void vector<_T,_Allocator>::assign(InputIt __first, InputIt __last)
{
    resize(0);
    while (__first != __last) {
        push_back(*__first);
        ++__first;
    }
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::assign(initializer_list<_T> __ilist)
{
    assign(__ilist.begin(), __ilist.end());
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
    return __m[0];
}

template<typename _T, typename _Allocator>
_T const&
vector<_T,_Allocator>::front() const
{
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
        uninitialized_move(__m, __m + __sz, new_p.get());
        for (size_t i = 0; i < __sz; ++i)
            __m[i].~value_type();
        if (__m != nullptr)
            __alloc.deallocate(__m, __sz);
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
        pointer __tmp = __alloc.allocate(__sz * sizeof(value_type));
        uninitialized_copy(__m, __m + __sz, __tmp);
        __alloc.deallocate(__m);
        __m = __tmp;
        __sz = __capacity;
    }
}

template<typename _T, typename _Allocator>
void vector<_T,_Allocator>::clear()
{
    while (__sz > 0)
        pop_back();
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos, _T const& __value)
{
    iterator __it(__pos.__p);

    constexpr size_t __count = 1;

    new (__make_space(__pos, __count)) value_type(__value);

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
    iterator __it = iterator(__pos.p);
    pointer place = __make_space(__it, __count);
    uninitialized_fill(place, place + __count, __value);

    __sz += __count;

    return __it;
}

template<typename _T, typename _Allocator>
template<typename InputIt>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos,
                              InputIt __first, InputIt __last)
{
    iterator __it(__pos.__p);
    constexpr size_t __count = __last - __first;
    pointer place = __make_space(__it, __count);
    uninitialized_copy(__first, __last, place);

    return __it;
}

template<typename _T, typename _Allocator>
typename vector<_T,_Allocator>::iterator
vector<_T,_Allocator>::insert(const_iterator __pos,
                              initializer_list<_T> __ilist)
{
    return insert(iterator(__pos.__p), __ilist.begin(), __ilist.end());
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
    constexpr size_t __count = __last - __first;
    pointer __p = __first.__p;
    pointer __e = __m + __sz - __count;
    for ( ; __p < __e; ++__p) {
        __alloc.destruct(__p);
        new (__p) value_type(move(__p[__count]));
    }
    __alloc.destruct(__e);
    __sz -= __count;
    return iterator(__first);
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
bool vector<_T,_Allocator>::push_back(_T const& __value)
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
bool vector<_T,_Allocator>::push_back(_T&& __value)
{
    if (unlikely(__sz + 1 > __capacity)) {
        if (unlikely(!__grow()))
            return false;
    }

    new (__m + __sz) value_type(move(__value));

    ++__sz;

    return true;
}

template<typename _T, typename _Allocator>
template<typename... _Args>
bool vector<_T,_Allocator>::emplace_back(_Args&& ...__args)
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
void vector<_T,_Allocator>::pop_back()
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
        do {
            pop_back();
        } while (--__sz > __count);
    } else {
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
    ::swap(__m, __other.__m);
    ::swap(__sz, __other.__sz);
    ::swap(__capacity, __other.__capacity);
    ::swap(__alloc, __other.__alloc);
}

//
// Comparison

template< typename _R, typename _Alloc >
bool operator==(vector<_R,_Alloc> const& lhs,
                vector<_R,_Alloc> const& rhs)
{
    return equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template< typename _R, typename _Alloc >
bool operator!=(vector<_R,_Alloc> const& lhs,
                vector<_R,_Alloc> const& rhs)
{
    return !equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
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
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::vector_iter(
        vector_iter<_Dir, _RhsIsConst> const& rhs)
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
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::vector_iter(pointer __ip)
    : __p(__ip)
{
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator *()
{
    return *__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T const&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator *() const
{
    return *__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator+(
        size_type __n) const
{
    return vector_iter(__p + __n * _Dir);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator-(
        size_type __n) const
{
    return vector_iter(__p - __n * _Dir);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator[](size_type __n)
{
    return __p[__n];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
_T const&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator[](size_type __n) const
{
    return __p[__n];
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::pointer
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator->()
{
    return __p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::const_pointer
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator->() const
{
    return __p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator==(
        vector_iter const& rhs) const
{
    return __p == rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator!=(
        vector_iter const& rhs) const
{
    return __p != rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator<(
        vector_iter const& rhs) const
{
    return __p < rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator<=(
        vector_iter const& rhs) const
{
    return __p <= rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator>=(
        vector_iter const& rhs) const
{
    return __p >= rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
bool vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator>(
        vector_iter const& rhs) const
{
    return __p > rhs.__p;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator++()
{
    ++__p;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator++(int)
{
    return vector_iter(__p++);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator--()
{
    ++__p;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator--(int)
{
    return vector_iter(__p--);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator+=(
        difference_type diff)
{
    __p += diff;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>&
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator-=(
        difference_type diff)
{
    __p -= diff;
    return *this;
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator+(
        difference_type diff) const
{
    return vector_iter(__p + diff);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::template vector_iter<_Dir, _Is_const>
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator-(
        difference_type diff) const
{
    return vector_iter(__p - diff);
}

template<typename _T, typename _Alloc>
template<int _Dir, bool _Is_const>
typename vector<_T,_Alloc>::difference_type
vector<_T,_Alloc>::vector_iter<_Dir, _Is_const>::operator-(
        vector_iter const& rhs) const
{
    return __p - rhs.__p;
}
