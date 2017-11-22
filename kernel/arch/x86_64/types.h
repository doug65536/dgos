#pragma once

#include <stdint-gcc.h>
#include <stddef.h>
#include <limits.h>

#if defined(__GNUC__)

#define __packed                __attribute__((packed))
#define __const                 __attribute__((const))
#define __pure                  __attribute__((pure))
#define __aligned(n)            __attribute__((aligned(n)))
#define __always_inline         __attribute__((always_inline)) inline
#define __noreturn              __attribute__((noreturn))
#define __used                  __attribute__((used))
#define __returns_twice         __attribute__((returns_twice))
#define __vector_size(n)        __attribute__((vector_size(n)))
#define __noinline              __attribute__((noinline))
#define __assume_aligned(n)     __attribute__((assume_aligned(n)))
#define __printf_format(m,n)    __attribute__((format(printf, m, n)))
#define __malloc                __attribute__((malloc))
#define __section(name)         __attribute__((section(name)))

typedef int_fast64_t off_t;
typedef __SIZE_TYPE__ size_t;
typedef long ssize_t;

#endif

/// SSE vectors
typedef int8_t __vector_size(16) __i8_vec16;
typedef int16_t __vector_size(16) __i16_vec8;
typedef int32_t __vector_size(16) __i32_vec4;
typedef int64_t __vector_size(16) __i64_vec2;
typedef uint8_t __vector_size(16) __u8_vec16;
typedef uint16_t __vector_size(16) __u16_vec8;
typedef uint32_t __vector_size(16) __u32_vec4;
typedef uint64_t __vector_size(16) __u64_vec2;
typedef float __vector_size(16) __f32_vec4;
typedef double __vector_size(16) __d64_vec2;

// Some builtins unnecessarily insist on long long types
typedef long long __vector_size(16) __i64_vec2LL;
typedef unsigned long long __vector_size(16) __ivec2ULL;

#define countof(arr) (sizeof(arr)/sizeof(*arr))

/*
#define CHAR_BIT        8

#define MB_LEN_MAX      4

#define CHAR_MIN        -128
#define CHAR_MAX        127

#define SCHAR_MIN       -128
#define SCHAR_MAX       127

#define SHRT_MIN        -32768
#define SHRT_MAX        32767

#define INT_MIN         -2147483648
#define INT_MAX         2147483647

#define LONG_MIN        -9223372036854775808L
#define LONG_MAX        9223372036854775807L

#define LLONG_MIN       -9223372036854775808L
#define LLONG_MAX       9223372036854775807L

#define UCHAR_MAX       255U
#define USHRT_MAX       65535U
#define UINT_MAX        4294967295U
#define ULONG_MAX       18446744073709551615UL
#define ULLONG_MAX      18446744073709551615ULL

#define PTRDIFF_MAX     (-__PTRDIFF_MAX__-1)
#define PTRDIFF_MIN     __PTRDIFF_MAX__
#define SIZE_MAX        __SIZE_MAX__
#define SIG_ATOMIC_MIN  __SIG_ATOMIC_MIN__
#define SIG_ATOMIC_MAX  __SIG_ATOMIC_MAX__
#define WCHAR_MIN       __WCHAR_MIN__
#define WCHAR_MAX       __WCHAR_MAX__
#define WINT_MIN        __WINT_MIN__
#define WINT_MAX        __WINT_MAX__

#define INT_FAST8_MAX   __INT_FAST8_MAX__
#define INT_FAST8_MIN   (-__INT_FAST8_MAX__-1)
#define INT_FAST16_MAX  __INT_FAST16_MAX__
#define INT_FAST16_MIN  (-__INT_FAST16_MAX__-1)
#define INT_FAST32_MAX  __INT_FAST32_MAX__
#define INT_FAST32_MIN  (-__INT_FAST32_MAX__-1)
#define INT_FAST64_MAX  __INT_FAST64_MAX__
#define INT_FAST64_MIN  (-__INT_FAST64_MAX__-1)
#define INT_LEAST8_MAX  __INT_LEAST8_MAX__
#define INT_LEAST8_MIN  (-__INT_LEAST8_MAX__-1)
#define INT_LEAST16_MAX __INT_LEAST16_MAX__
#define INT_LEAST16_MIN (-__INT_LEAST16_MAX__-1)
#define INT_LEAST32_MAX __INT_LEAST32_MAX__
#define INT_LEAST32_MIN (-__INT_LEAST32_MAX__-1)
#define INT_LEAST64_MAX __INT_LEAST64_MAX__
#define INT_LEAST64_MIN (-__INT_LEAST64_MAX__-1)
#define INT8_MAX        __INT8_MAX__
#define INT8_MIN        (-__INT8_MAX__-1)
#define INT16_MAX       __INT16_MAX__
#define INT16_MIN       (-__INT16_MAX__-1)
#define INT32_MAX       __INT32_MAX__
#define INT32_MIN       (-__INT32_MAX__-1)
#define INT64_MAX       __INT64_MAX__
#define INT64_MIN       (-__INT64_MAX__-1)
#define INTMAX_MAX      __INTMAX_MAX__
#define INTMAX_MIN      (-__INTMAX_MAX__-1)
#define INTPTR_MAX      __INTPTR_MAX__
#define INTPTR_MIN      (-__INTPTR_MAX__-1)

#define UINT_FAST8_MAX   __UINT_FAST8_MAX__
#define UINT_FAST8_MIN   0
#define UINT_FAST16_MAX  __UINT_FAST16_MAX__
#define UINT_FAST16_MIN  0
#define UINT_FAST32_MAX  __UINT_FAST32_MAX__
#define UINT_FAST32_MIN  0
#define UINT_FAST64_MAX  __UINT_FAST64_MAX__
#define UINT_FAST64_MIN  0
#define UINT_LEAST8_MAX  __UINT_LEAST8_MAX__
#define UINT_LEAST8_MIN  0
#define UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#define UINT_LEAST16_MIN 0
#define UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#define UINT_LEAST32_MIN 0
#define UINT_LEAST64_MAX __UINT_LEAST64_MAX__
#define UINT_LEAST64_MIN 0
#define UINT8_MAX        __UINT8_MAX__
#define UINT8_MIN        0
#define UINT16_MAX       __UINT16_MAX__
#define UINT16_MIN       0
#define UINT32_MAX       __UINT32_MAX__
#define UINT32_MIN       0
#define UINT64_MAX       __UINT64_MAX__
#define UINT64_MIN       0
#define UINTMAX_MAX      __UINTMAX_MAX__
#define UINTMAX_MIN      0
#define UINTPTR_MAX      __UINTPTR_MAX__
#define UINTPTR_MIN      0
*/
