#pragma once

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

typedef uint64_t size_t;
typedef int64_t ssize_t;

typedef uint64_t uintptr_t;
typedef int64_t intptr_t;

typedef int64_t off_t;

typedef int wint_t;

typedef int64_t ptrdiff_t;
typedef uint64_t uptrdiff_t;

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

typedef int wchar_t;

/// SSE vectors
typedef int8_t __ivec16 __attribute__((vector_size(16)));
typedef int16_t __ivec8 __attribute__((vector_size(16)));
typedef int32_t __ivec4 __attribute__((vector_size(16)));
typedef int64_t __ivec2 __attribute__((vector_size(16)));
typedef uint8_t __uvec16 __attribute__((vector_size(16)));
typedef uint16_t __uvec8 __attribute__((vector_size(16)));
typedef uint32_t __uvec4 __attribute__((vector_size(16)));
typedef uint64_t __uvec2 __attribute__((vector_size(16)));
typedef float __fvec4 __attribute__((vector_size(16)));
typedef float __dvec2 __attribute__((vector_size(16)));

#define countof(arr) (sizeof(arr)/sizeof(*arr))

// LOL
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

#define PTRDIFF_MIN     LONG_MIN
#define PTRDIFF_MAX     LONG_MAX
#define SIZE_MAX        ULONG_MAX
#define SIG_ATOMIC_MIN  LONG_MIN
#define SIG_ATOMIC_MAX  LONG_MAX
#define WCHAR_MIN       INT_MIN
#define WCHAR_MAX       INT_MAX
#define WINT_MIN        INT_MIN
#define WINT_MAX        INT_MAX
