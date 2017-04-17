#pragma once

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

typedef __SIZE_TYPE__ size_t;
typedef int16_t ssize_t;

typedef uint16_t uintptr_t;
typedef int16_t intptr_t;

typedef int64_t off_t;

#define __stdcall               __attribute__((stdcall))

#define __packed                __attribute((packed))
#define __const                 __attribute__((const))
#define __pure                  __attribute__((pure))
#define __aligned(n)            __attribute__((aligned(n)))
#define __always_inline         inline __attribute__((always_inline))
#define __noreturn              __attribute__((noreturn))
#define __used                  __attribute__((used))
#define __returns_twice         __attribute__((returns_twice))
#define __vector_size(n)        __attribute__((vector_size(n)))
#define __noinline              __attribute__((noinline))
#define __assume_aligned(n)     __attribute__((assume_aligned(n)))
#define __printf_format(m,n)    __attribute__((format(printf, m, n)))
