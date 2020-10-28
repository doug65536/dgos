#pragma once

#if 1

#if defined(__INT8_TYPE__) && defined(__INT16_TYPE__) && \
    defined(__INT32_TYPE__) && defined(__INT64_TYPE__) && \
    defined(__UINT8_TYPE__) && defined(__UINT16_TYPE__) && \
    defined(__UINT32_TYPE__) && defined(__UINT64_TYPE__)

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __INTMAX_TYPE__ intmax_t;

typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __SIZE_TYPE__ size_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __UINTMAX_TYPE__ uintmax_t;

typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

#define UINT8_C(n) __UINT8_C(n)
#define UINT16_C(n) __UINT16_C(n)
#define UINT32_C(n) __UINT32_C(n)
#define UINT64_C(n) __UINT64_C(n)

#define INT8_C(n) __INT8_C(n)
#define INT16_C(n) __INT16_C(n)
#define INT32_C(n) __INT32_C(n)
#define INT64_C(n) __INT64_C(n)

#define UINT8_MAX __UINT8_MAX__
#define UINT16_MAX __UINT16_MAX__
#define UINT32_MAX __UINT32_MAX__
#define UINT64_MAX __UINT64_MAX__
#define SIZE_MAX __SIZE_MAX__
#define UINTPTR_MAX __UINTPTR_MAX__
#define UINTMAX_MAX __UINTMAX_MAX__

#define INT8_MAX __INT8_MAX__
#define INT16_MAX __INT16_MAX__
#define INT32_MAX __INT32_MAX__
#define INT64_MAX __INT64_MAX__
#define INTPTR_MAX __INTPTR_MAX__
#define PTRDIFF_MAX __PTRDIFF_MAX__
#define INTMAX_MAX __INTMAX_MAX__

#define INT8_MIN    (-__INT8_MAX__   -1)
#define INT16_MIN   (-__INT16_MAX__  -1)
#define INT32_MIN   (-__INT32_MAX__  -1)
#define INT64_MIN   (-__INT64_MAX__  -1)
#define INTPTR_MIN  (-__INTPTR_MAX__ -1)
#define PTRDIFF_MIN (-__PTRDIFF_MAX__-1)
#define INTMAX_MIN  (-__INTMAX_MAX__ -1)

#ifdef __WCHAR_MAX__
#define __WCHAR_MAX__ __INT32_MAX__
#endif
#ifdef __WCHAR_MIN__
#define __WCHAR_MIN__ (-__INT32_MAX__-1)
#endif

#elif defined(__x86_64__)
// Hardcoded because compilers barely implement freestanding
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef unsigned long uintmax_t;
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;
typedef long intptr_t;
typedef long intmax_t;
typedef long ptrdiff_t;
#elif defined(__i386__)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef unsigned long long uintmax_t;
typedef char int8_t;
typedef short int16_t;
typedef long int32_t;
typedef long long int64_t;
typedef long intptr_t;
typedef long long intmax_t;
typedef long ptrdiff_t;
#endif

#else

#ifdef __clang__
#include_next <stdint.h>
#else
#include "stdint-gcc.h"
#endif

#endif
