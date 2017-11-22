#pragma once

#include <stdint-gcc.h>
#include <stddef.h>

typedef long ssize_t;
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
