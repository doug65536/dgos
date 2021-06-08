#pragma once

#include "generic_types.h"

#define _generic_target \
    __attribute__(( \
    __target__("no-sse3"), \
    __target__("no-ssse3"), \
    __target__("no-sse4.1"), \
    __target__("no-sse4.2"), \
    __target__("no-avx"), \
    __target__("no-avx2") \
    ))

#if 0
typedef __UINT64_TYPE__ uint64_t;
typedef __INT64_TYPE__ int64_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT32_TYPE__ int32_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __INT16_TYPE__ int16_t;
typedef __UINT8_TYPE__ uint8_t;
typedef __INT8_TYPE__ int8_t;

typedef __UINT_FAST64_TYPE__ uint_fast64_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __INT_FAST8_TYPE__ int_fast8_t;

typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTPTR_TYPE__ intptr_t;

#endif

