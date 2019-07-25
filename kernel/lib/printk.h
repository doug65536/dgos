#pragma once

#include <stdarg.h>
#include "types.h"
#include <inttypes.h>
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

class hex {};

class setw { public: setw(int w) : w(w) {} int const w; };

class setfill {};

__END_NAMESPACE_STD

class debug_out_t {
public:
    template<typename T>
    typename std::enable_if<
        std::is_integral<T>::value && std::is_unsigned<T>::value,
        debug_out_t&
    >::type
    operator<<(T const& rhs)
    {
        return write_unsigned((uint64_t)rhs, sizeof(T));
    }

    template<typename T>
    typename std::enable_if<
        std::is_integral<T>::value && std::is_signed<T>::value,
        debug_out_t&
    >::type
    operator<<(T const& rhs)
    {
        return write_signed((int64_t)rhs, sizeof(T));
    }

    debug_out_t& operator<<(char const *rhs);
    debug_out_t& operator<<(bool const& value);

    debug_out_t& operator<<(std::hex);

private:
    bool hex = false;

    debug_out_t& write_unsigned(uint64_t value, size_t sz);

    debug_out_t& write_signed(int64_t value, size_t sz);

    debug_out_t& write_str(char const *rhs);
};


extern debug_out_t dbgout;
