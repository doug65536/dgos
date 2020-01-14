#pragma once

#include <stdarg.h>
#include "types.h"
#include <inttypes.h>
#include "utility.h"

//#include "cxxexcept.h"

__BEGIN_DECLS

_noreturn
void vpanic(char const * restrict format, va_list ap);

_noreturn
void panic(char const *format, ...)
    _printf_format(1, 2);

_noreturn
void panic_oom();

int vsnprintf(char * restrict buf, size_t limit,
              char const * restrict format, va_list ap);

int snprintf(char * restrict buf, size_t limit,
             char const * restrict format, ...)
    _printf_format(3, 4);

void printk(char const *format, ...)
    _printf_format(1, 2);

void vprintk(char const *format, va_list ap);

int printdbg(char const * restrict format, ...)
    _printf_format(1, 2);

int vprintdbg(char const * restrict format, va_list ap);

int cprintf(char const * restrict format, ...)
    _printf_format(1, 2);

int vcprintf(char const * restrict format, va_list ap);

int hex_dump(void const volatile *mem, size_t size, uintptr_t base = 0);

int putsdbg(char const *s);

int writedbg(char const *s, size_t len);

struct format_flag_info_t {
    char const * const name;
    uintptr_t mask;
    char const * const *value_names;
    int8_t bit;
    uint64_t :56;
};

size_t format_flags_register(
        char *buf, size_t buf_size,
        uintptr_t flags, format_flag_info_t const *info);

intptr_t formatter(
        char const *format, va_list ap,
        int (*emit_chars)(char const *, intptr_t, void*),
        void *emit_context);

__END_DECLS

#include "type_traits.h"

__BEGIN_NAMESPACE_STD

class setw { public: setw(int w) : w(w) {} int const w; };

class setfill {};

__END_NAMESPACE_STD

enum debug_out_modifier_t { dec, hex, plus, noplus };

class debug_out_t {
public:
    // unsigned
    template<typename T>
    _always_inline typename std::enable_if<
        std::is_integral<T>::value && std::is_unsigned<T>::value &&
        !std::is_same<T, bool>::value && !std::is_same<T, char>::value,
        debug_out_t&
    >::type
    operator<<(T const& rhs)
    {
        return write_unsigned(uint64_t(rhs), sizeof(T));
    }

    // signed
    template<typename T>
    _always_inline typename std::enable_if<
        std::is_integral<T>::value && std::is_signed<T>::value &&
        !std::is_same<T, bool>::value && !std::is_same<T, char>::value,
        debug_out_t&
    >::type
    operator<<(T const& rhs)
    {
        return write_signed(int64_t(rhs), sizeof(T));
    }

    // string literal
    template<size_t _N>
    _always_inline debug_out_t& operator<<(char const (&str)[_N])
    {
        writedbg(str, _N - 1);
        return *this;
    }

    // char const *
    template<typename T>
    _always_inline typename std::enable_if<std::is_same<T,
    char const *>::value, debug_out_t&>::type
    operator<<(T str)
    {
        return write_str(str);
    }

    // pointer other than const char *
    template<typename T>
    _always_inline typename std::enable_if<std::is_pointer<T>::value &&
    !std::is_same<T, char const *>::value, debug_out_t&>::type
    operator<<(T ptr)
    {
        return write_ptr(ptr);
    }

    template<typename _K, typename _V>
    debug_out_t& operator<<(std::pair<_K, _V> const& __rhs)
    {
        return *this << '{' << __rhs.first << ',' << __rhs.second << '}';
    }

    template<typename _F, typename _R,
             typename... _A>
    debug_out_t& operator<<(_R (*__f)(_A...))
    {
        return *this << '<' << uintptr_t(__f) << '>';
    }

    // nullptr
    _always_inline debug_out_t& operator<<(nullptr_t n)
    {
        return write_str("<nullptr>");
    }

    _always_inline debug_out_t& operator<<(bool b)
    {
        return write_bool(b);
    }

    _always_inline debug_out_t&operator<<(char c)
    {
        return write_char(c);
    }

    debug_out_t& operator<<(debug_out_modifier_t);

private:
    bool use_hex = false;
    bool use_plus = false;

    debug_out_t& write_unsigned(uint64_t value, size_t sz);
    debug_out_t& write_signed(int64_t value, size_t sz);
    debug_out_t& write_bool(bool value);
    debug_out_t& write_char(char value);
    debug_out_t& write_str(char const *rhs);
    debug_out_t& write_ptr(void const *rhs);
};

extern debug_out_t dbgout;
