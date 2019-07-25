#pragma once
#include "types.h"
#include "vector.h"
#include "hash.h"
#include "cxxexception.h"

__BEGIN_NAMESPACE_STD

template<typename _CharT>
class char_traits
{
public:
    using char_type = _CharT;
    using int_type = int;

    static inline constexpr void assign(char_type& __r, char_type const& __a)
    {
        __r = __a;
    }

    static inline char_type* assign(
            char_type* __p, size_t __count, char_type __a)
    {
        fill_n(__p, __count, __a);
        return __p;
    }

    static inline constexpr bool eq(char_type __a, char_type __b)
    {
        return __a == __b;
    }

    static inline constexpr bool lt(char_type __a, char_type __b)
    {
        return __a < __b;
    }

    static char_type* move(char_type* __dest,
                           char_type const* __src, size_t __count);

    static char_type* copy(char_type* __dest,
                           char_type const* __src, size_t __count)
    {
        return std::copy(__src, __src + __count, __dest);
    }

    static int compare(_CharT const *__lhs, _CharT const *__rhs, size_t __rlen)
    {
        for (size_t __chk = 0; __chk < __rlen; ++__chk) {
            if (__lhs[__chk] < __rhs[__chk])
                return -1;
            if (__lhs[__chk] > __rhs[__chk])
                return 1;
        }
        return 0;
    }

    static size_t length(char_type const* __rhs)
    {
        size_t __sz = 0;
        while (__rhs[__sz])
            ++__sz;
        return __sz;
    }

    static char_type const* find(char_type const* __rhs, size_t __count,
                                 char_type const& __ch);

    static inline constexpr char_type to_char_type(int_type c)
    {
        return char_type(c);
    }

    static inline constexpr int_type to_int_type(char_type c)
    {
        return int_type(c);
    }

    static inline constexpr bool eq_int_type(int_type c1, int_type c2)
    {
        return c1 == c2;
    }

    static inline constexpr int_type eof()
    {
        return -1;
    }

    static inline constexpr int_type not_eof(int_type e) noexcept
    {
        return e == eof() ? 0 : e;
    }

};

template<typename _CharT>
typename char_traits<_CharT>::char_type *
char_traits<_CharT>::move(
        char_type *__dest, char_type const *__src, size_t __count)
{
    if (__dest < __src || __src + __count <= __dest) {
        std::copy(__src, __src + __count, __dest);
    } else if (__dest > __src) {
        for (size_t __i = __count; __i > 0; --__i)
            __dest[__i] = __src[__i];
    }
    return __dest;
}

template<typename _CharT>
typename char_traits<_CharT>::char_type const *
char_traits<_CharT>::find(char_type const *__rhs, size_t __count,
                  char_type const &__ch)
{
    for (size_t __chk = 0; __chk < __count; ++__chk) {
        if (__rhs[__chk] == __ch)
            return __rhs + __chk;
    }
    return nullptr;
}

namespace detail {

_noreturn _noinline
void throw_bad_alloc();

}

template<typename _CharT,
typename _Traits = char_traits<_CharT>,
typename _Allocator = allocator<_CharT>>
class basic_string
{
private:
    using _Storage = vector<_CharT, _Allocator>;
public:
    static_assert(std::has_trivial_destructor<_CharT>::value,
                  "Null termination technique requires"
                  " trivially destructible type");

    using traits_type = _Traits;
    using value_type = _CharT;
    using allocator_type = _Allocator;
    using size_type = typename _Allocator::size_type;
    using difference_type = typename _Allocator::difference_type;
    using reference = typename _Allocator::reference;
    using const_reference = typename _Allocator::const_reference;
    using pointer = typename _Allocator::pointer;
    using const_pointer = typename _Allocator::const_pointer;
    using iterator = typename _Storage::iterator;
    using const_iterator = typename _Storage::const_iterator;
    using reverse_iterator = typename _Storage::reverse_iterator;
    using const_reverse_iterator = typename _Storage::const_reverse_iterator;

    static constexpr size_type npos = -1;

    basic_string()
    {
    }

    basic_string(_CharT const *__rhs)
    {
        size_type __sz = traits_type::length(__rhs);
        if (unlikely(!__str.reserve(__sz + 1)))
            detail::throw_bad_alloc();
        __str.assign(__rhs, __rhs + __sz);
        __str[__sz] = 0;
    }

    basic_string(basic_string const& __rhs)
        : __str(__rhs.__str)
    {
    }

    basic_string(basic_string&& __rhs) noexcept
        : __str(move(__rhs.__str))
    {
    }

    basic_string(_CharT const *__st, _CharT const *__en)
    {
        size_type __sz = __en - __st;
        if (unlikely(!__str.reserve(__sz + 1)))
            detail::throw_bad_alloc();
        __str.assign(__st, __en);
        __str[__sz] = 0;
    }

    basic_string(size_type __n, _CharT __value)
    {
        if (unlikely(!__str.reserve(__n + 1)))
            detail::throw_bad_alloc();
        __str.assign(__n, __value);
        __str[__n] = 0;
    }

    template<typename _IterT,
             typename _IterV = decltype(*declval<_IterT>())>
    basic_string(_IterT __st, _IterT __en)
    {
        size_type __sz = __en - __st;
        if (unlikely(!__str.reserve(__sz + 1)))
            detail::throw_bad_alloc();

        __str.assign(__st, __en);
        __str[__sz] = 0;
    }

    basic_string &operator=(basic_string __rhs)
    {
        __str = move(__rhs.__str);
        return *this;
    }

    basic_string &operator=(_CharT const *__rhs)
    {
        size_type __sz = traits_type::length(__rhs);
        if (unlikely(!__str.reserve(__sz + 1)))
            detail::throw_bad_alloc();

        __str.assign(__rhs, __rhs + __sz);
        __str[__sz] = 0;
        return *this;
    }

    static constexpr const _CharT __empty_str[1] = {};

    _CharT const *c_str() const
    {
        return !__str.empty() ? __str.data() : __empty_str;
    }

    operator bool() const
    {
        return __str.size() > 1;
    }

    iterator begin()
    {
        return __str.begin();
    }

    const_iterator begin() const
    {
        return __str.cbegin();
    }

    const_iterator cbegin() const
    {
        return __str.cbegin();
    }

    iterator end()
    {
        return __str.end();
    }

    const_iterator end() const
    {
        return __str.cend();
    }

    const_iterator cend() const
    {
        return __str.cend();
    }

    _CharT& front()
    {
        return __str.front();
    }

    _CharT const& front() const
    {
        return __str.front();
    }

    _CharT& back()
    {
        return __str.back();
    }

    _CharT const& back() const
    {
        return __str.back();
    }

    _CharT *data()
    {
        return __str.data();
    }

    _CharT const *data() const
    {
        return __str.data();
    }

    _CharT& operator[](size_type __index)
    {
        return __str[__index];
    }

    _CharT const& operator[](size_type __index) const
    {
        return __str[__index];
    }

    _CharT& at(size_type __index)
    {
        return __str.at(__index);
    }

    _CharT const& at(size_type __index) const
    {
        return __str.at(__index);
    }

    bool empty() const
    {
        return __str.empty();
    }

    size_type size() const
    {
        return __str.size();
    }

    size_type length() const
    {
        return __str.size();
    }

    size_type max_size() const
    {
        return __str.max_size();
    }

    bool reserve(size_type __sz)
    {
        return __str.reserve(__sz);
    }

    size_type capacity() const
    {
        return __str.capacity();
    }

    bool resize(size_type __sz)
    {
        return __str.resize(__sz);
    }

    bool resize(size_type __sz, _CharT __ch)
    {
        return __str.resize(__sz, __ch);
    }

    void shrink_to_fit()
    {
        return __str.shrink_to_fit();
    }

    void clear()
    {
        __str.clear();
    }

    bool push_back(_CharT __ch)
    {
        return __str.push_back(__ch);
    }

    void pop_back()
    {
        __str.pop_back();
    }

    basic_string& erase(size_type __index, size_type __count = npos)
    {
        size_type __removed = min(__count, __str.size() - __index);
        size_type __src = __index + __removed;
        for (size_type __i = 0; __i != __removed; ++__i)
            __str[__index + __i] = move(__str[__src + __i]);
        resize(__str.size() - __removed);
        return *this;
    }

    iterator erase(const_iterator __pos)
    {
        return __str.erase(__pos);
    }

    iterator erase(const_iterator __first, const_iterator __last)
    {
        return __str.erase(__first, __last);
    }

    void swap(basic_string& __rhs)
    {
        __str.swap(__rhs.__str);
    }

    size_type copy(_CharT* __dest, size_type __count, size_type __pos = 0) const
    {
        if (__str.size() <= __pos)
            return 0;

        size_type __max_count = __str.size() - __pos;

        if (__count > __max_count)
            __count = __max_count;

        std::copy(__str.data() + __pos, __str.data() + __pos + __count, __dest);

        return __count;
    }

    basic_string& append(size_type __count, _CharT __ch)
    {
        if (!__str.resize(__count - __str.size(), __ch))
            detail::throw_bad_alloc();

        return *this;
    }

    basic_string& append(basic_string const& __rhs)
    {
        if (unlikely(!__str.reserve(__str.size() + __rhs.size())))
            detail::throw_bad_alloc();

        for (_CharT const& __ch : __rhs)
            __str.push_back(__ch);

        return *this;
    }

    basic_string& operator+=(basic_string const& __rhs)
    {
        return append(__rhs);
    }

    basic_string& operator+=(_CharT const& __rhs)
    {
        return append(size_type(1), __rhs);
    }

    basic_string& operator+=(initializer_list<_CharT> __ilist)
    {
        return append(__ilist.begin(), __ilist.end());
    }

    basic_string& append(basic_string const& __rhs,
                         size_type __pos, size_type __count = npos )
    {
        if (__pos >= __rhs.size())
            return *this;

        size_type __max_count = __rhs.size() - __pos;

        if (__count > __max_count)
            __count = __max_count;

        if (unlikely(!__str.reserve(__str.size() + __count)))
            detail::throw_bad_alloc();

        const_iterator __end = __rhs.begin() + (__pos + __count);

        for (const_iterator __src = __rhs.cbegin(); __src != __end; ++__src)
            __str.push_back(*__src);

        return *this;
    }

    basic_string& append(_CharT const* __s, size_type __count)
    {
        _CharT const *__end = __s + __count;
        size_type __sz = __end - __s;
        if (unlikely(!__str.reserve(__str.size() + __sz)))
            detail::throw_bad_alloc();

        for ( ; __s != __end; ++__s)
            __str.push_back(*__s);

        return *this;
    }

    basic_string& append(_CharT const* __s)
    {
        _CharT const *__end;
        for (__end = __s; *__end; ++__end);
        size_type __sz = __end - __s;

        if (unlikely(!__str.reserve(__str.size() + __sz)))
            detail::throw_bad_alloc();

        for ( ; __s != __end; ++__s)
            __str.push_back(*__s);
        return *this;
    }

    template<typename _InputIt >
    basic_string& append(_InputIt __first, _InputIt __last)
    {
        if (unlikely(!__str.reserve(__str.size() + (__last - __first))))
            detail::throw_bad_alloc();

        for ( ; __first != __last; ++__first)
            __str.push_back(*__first);

        return *this;
    }

    basic_string& append(initializer_list<_CharT> __ilist)
    {
        return append(__ilist.begin(), __ilist.end());
    }

    int compare(basic_string const& __rhs) const
    {
        size_type __rlen = min(__str.size(), __rhs.size());

        int __cmp = traits_type::compare(__str.data(), __rhs.data(), __rlen);

        if (__cmp < 0)
            return -1;

        if (__cmp > 0)
            return 1;

        if (__str.size() < __rhs.size())
            return -1;

        if (__str.size() > __rhs.size())
            return 1;

        return 0;
    }

    int compare(size_type __pos1, size_type __count1,
                 basic_string const& __rhs) const
    {
        return compare(__pos1, __count1, __rhs, size_type(0));
    }

    int compare(size_type __pos1, size_type __count1,
                 basic_string const& __rhs,
                 size_type __pos2, size_type __count2 = npos) const
    {
        size_type __max_count1 = __pos1 <= __str.size()
                ? __str.size() - __pos1 : 0;

        size_type __max_count2 = __pos2 <= __rhs.size()
                ? __rhs.size() - __pos2 : 0;

        if (__count1 > __max_count1)
            __count1 = __max_count1;

        if (__count2 > __max_count2)
            __count2 = __max_count2;

        size_type __rlen = min(__count1, __count2);

        int __cmp = traits_type::compare(__str.data() + __pos1,
                                         __rhs.data() + __pos2, __rlen);

        if (__cmp < 0)
            return -1;

        if (__cmp > 0)
            return 1;

        if (__count1 < __count2)
            return -1;

        if (__count1 > __count2)
            return 1;

        return 0;
    }

    int compare(_CharT const* __rhs) const
    {
        size_type __count2 = 0;
        while (__rhs[__count2])
            ++__count2;

        return compare(0, __str.size(), __rhs, __count2);
    }

    int compare(size_type __pos1, size_type __count1,
                 _CharT const* __rhs) const
    {
        size_type __count2 = 0;
        while (__rhs[__count2])
            ++__count2;
        return compare(__pos1, __count1, __rhs, __count2);
    }

    int compare(size_type __pos1, size_type __count1,
                 _CharT const* __rhs, size_type __count2) const
    {
        size_type __max_count1 = __pos1 <= __str.size()
                ? __str.size() - __pos1 : 0;

        if (__count1 > __max_count1)
            __count1 = __max_count1;

        size_type __rlen = min(__count1, __count2);

        int __cmp = traits_type::compare(__str.data() + __pos1, __rhs, __rlen);

        if (__cmp < 0)
            return -1;

        if (__cmp > 0)
            return 1;

        if (__count1 < __count2)
            return -1;

        if (__count1 > __count2)
            return 1;

        return 0;
    }

    size_type find(basic_string const& __rhs, size_type __pos = 0) const
    {
        return find(__rhs.data(), __pos, __rhs.size());
    }

    size_type find(_CharT const* __rhs, size_type __pos,
                   size_type __count) const
    {
        // Match is impossible if search string is longer than string
        if (__count > __str.size())
            return npos;

        size_type __end_pos = __str.size() - __count;

        const_iterator const __str_st = __str.cbegin();

        size_t __rhs_size;
        for (__rhs_size = 0; __rhs[__rhs_size]; ++__rhs_size);

        for ( ; __pos <= __end_pos; ++__pos) {
            if (equal(__str_st + __pos, __str_st + __pos + __rhs_size,
                      __rhs, __rhs + __count))
                return __pos;
        }

        return npos;
    }

    size_type find(_CharT const* __rhs, size_type __pos = 0) const
    {
        size_type __sz = traits_type::length(__rhs);
        return find(__rhs, __pos, __sz);
    }

    size_type find(_CharT __ch, size_type __pos = 0) const
    {
        const_iterator __it = std::find(__str.cbegin() + __pos,
                                        __str.cend(), __ch);
        return __it != __str.cend() ? __it - __str.cbegin() : npos;
    }

    size_type rfind(basic_string const& __rhs, size_type __pos = 0) const
    {
        return rfind(__rhs.data(), __pos, __rhs.size());
    }

    size_type rfind(_CharT const* __rhs, size_type __pos,
                    size_type __count) const
    {
        // Match is impossible if search string is longer than string
        if (__count > __str.size())
            return npos;

        const_iterator const __str_st = __str.cbegin();

        for (size_type __srch = __str.size() - __count; ; --__srch) {
            if (equal(__str_st + __srch, __str_st + __srch + __count,
                      __rhs, __rhs + __count))
                return __srch;

            if (__srch == __pos)
                break;
        }

        return npos;
    }

    size_type rfind(_CharT const* __rhs, size_type __pos = 0) const
    {
        size_type __sz = traits_type::length(__rhs);
        return rfind(__rhs, __pos, __sz);
    }

    size_type rfind(_CharT __ch, size_type __pos = 0) const
    {
        for (size_type __srch = __str.size() - __pos; __srch; --__srch) {
            if (__str[__srch - 1] == __ch)
                return __srch - 1;
        }
        return npos;
    }

    size_type find_first_of(basic_string const& __rhs,
                            size_type __str_pos = 0) const
    {
        if (__str_pos < __str.size())
            return find_first_of(__rhs.data(), __str_pos, __rhs.size());
        return npos;
    }

    size_type find_first_of(_CharT const* __rhs,
                            size_type __str_pos, size_type __rhs_count) const
    {
        for (size_type __chk = __str_pos, __chk_end = __str.size();
             __chk < __chk_end; ++__chk) {
            for (size_type __cmp = 0; __cmp != __rhs_count; ++__cmp) {
                if (__str[__chk] == __rhs[__cmp])
                    return __chk;
            }
        }
        return npos;
    }

    size_type find_first_of(_CharT const* __rhs, size_type __str_pos = 0) const
    {
        size_type __rhs_count = traits_type::length(__rhs);
        return find_first_of(__rhs, __str_pos, __rhs_count);
    }

    size_type find_first_of(_CharT __ch, size_type __str_pos = 0) const
    {
        auto result = std::find(__str.cbegin() + __str_pos, __str.cend(), __ch);
        return result != __str.end() ? result - __str.begin() : npos;
    }

    size_type find_first_not_of(basic_string const& __rhs,
                                size_type __pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_first_not_of(_CharT const* __s,
                                size_type __str_pos, size_type __count) const
    {
        panic("Unimplemented");
    }

    size_type find_first_not_of(_CharT const* __s,
                                size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_first_not_of(_CharT __ch, size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_last_of(basic_string const& __rhs,
                           size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_last_of(_CharT const* __s,
                           size_type __str_pos, size_type __count) const
    {
        panic("Unimplemented");
    }

    size_type find_last_of(_CharT const* __s, size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_last_of(_CharT __ch, size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_last_not_of(basic_string const& __rhs,
                               size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_last_not_of(_CharT const* __s,
                               size_type __str_pos, size_type __count) const
    {
        panic("Unimplemented");
    }

    size_type find_last_not_of(_CharT const* __s,
                               size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

    size_type find_last_not_of(_CharT __ch, size_type __str_pos = 0) const
    {
        panic("Unimplemented");
    }

private:
    basic_string& __ensure_terminated()
    {
        if (__str.capacity() == __str.size())
            if (unlikely(!__str.reserve(__str.size() + 1)))
                detail::throw_bad_alloc();

        if (__str[__str.size()] == 0)
            __str[__str.size()] = 0;

        return *this;
    }

    vector<_CharT> __str;
};

template<typename _CharT, typename _Traits, typename _Alloc>
constexpr const _CharT basic_string<_CharT, _Traits, _Alloc>::__empty_str[1];

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator==(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __lhs.compare(__rhs) == 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator!=(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __lhs.compare(__rhs) != 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator<(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
               basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __lhs.compare(__rhs) < 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator<=(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __lhs.compare(__rhs) <= 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator>(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
               basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __lhs.compare(__rhs) > 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator>=(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __lhs.compare(__rhs) >= 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator==(_CharT const* __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __rhs.compare(__lhs) == 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator==(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                _CharT const* __rhs)
{
    return __lhs.compare(__rhs) == 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator!=(_CharT const* __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __rhs.compare(__lhs) != 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator!=(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                _CharT const* __rhs)
{
    return __lhs.compare(__rhs) != 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator<(_CharT const* __lhs,
               basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __rhs.compare(__lhs) > 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator<(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
               _CharT const* __rhs)
{
    return __lhs.compare(__rhs) < 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator<=(_CharT const* __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __rhs.compare(__lhs) >= 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator<=(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                _CharT const* __rhs)
{
    return __lhs.compare(__rhs) <= 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator>(_CharT const* __lhs,
               basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __rhs.compare(__lhs) < 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator>(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
               _CharT const* __rhs)
{
    return __lhs.compare(__rhs) > 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator>=(_CharT const* __lhs,
                basic_string<_CharT,_Traits,_Alloc> const& __rhs)
{
    return __rhs.compare(__lhs) <= 0;
}

template<typename _CharT, typename _Traits, typename _Alloc>
bool operator>=(basic_string<_CharT,_Traits,_Alloc> const& __lhs,
                _CharT const* __rhs)
{
    return __lhs.compare(__rhs) >= 0;
}

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;

template<typename _CharT, typename _Traits, typename _Alloc>
struct hash<basic_string<_CharT, _Traits, _Alloc>> {
    size_t operator()(std::basic_string<_CharT, _Traits, _Alloc> const& s) const
    {
        return hash_32(s.data(), s.size() * sizeof(*s.data()));
    }
};

string to_string(int value);
string to_string( long value );
string to_string( long long value );
string to_string( unsigned value );
string to_string( unsigned long value );
string to_string( unsigned long long value );
#ifndef __DGOS_KERNEL__
string to_string( float value );
string to_string( double value );
string to_string( long double value );
#endif

__END_NAMESPACE_STD

// Explicit instantiations
extern template class std::char_traits<char>;
extern template class std::char_traits<wchar_t>;
extern template class std::char_traits<char16_t>;
extern template class std::char_traits<char32_t>;
extern template class std::basic_string<char>;
extern template class std::basic_string<wchar_t>;
extern template class std::basic_string<char16_t>;
extern template class std::basic_string<char32_t>;
