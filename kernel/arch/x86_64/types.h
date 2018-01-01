#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef	__cplusplus
# define restrict
# define __BEGIN_DECLS	extern "C" {
# define __END_DECLS	}
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

#if defined(__GNUC__)

#define __packed                __attribute__((__packed__))
#define __const                 __attribute__((__const__))
#define __pure                  __attribute__((__pure__))
#define __aligned(n)            __attribute__((__aligned__(n)))
#define __always_inline         __attribute__((__always_inline__)) inline
#define __noreturn              __attribute__((__noreturn__))
#define __used                  __attribute__((__used__))
#define __returns_twice         __attribute__((__returns_twice__))
#define __vector_size(n)        __attribute__((__vector_size__(n)))
#define __noinline              __attribute__((__noinline__))
#define __assume_aligned(n)     __attribute__((__assume_aligned__(n)))
#define __printf_format(m,n)    __attribute__((__format__(printf, m, n)))
#define __malloc                __attribute__((__malloc__))
#define __section(name)         __attribute__((__section__(name)))
#define __artificial            __attribute__((__artificial__))

#define __generic_target \
    __attribute__(( \
    __target__("no-sse3"), \
    __target__("no-ssse3"), \
    __target__("no-sse4.1"), \
    __target__("no-sse4.2"), \
    __target__("no-avx"), \
    __target__("no-avx2") \
    ))

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

typedef int pid_t;

struct process_t;
