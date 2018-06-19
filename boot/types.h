#pragma once

#include <stdint.h>
#include <stddef.h>
#include "inttypes.h"

typedef long ssize_t;
typedef int64_t off_t;

#define __stdcall               __attribute__((__stdcall__))

#define __packed                __attribute((__packed__))
#define __const                 __attribute__((__const__))
#define __pure                  __attribute__((__pure__))
#define __aligned(n)            __attribute__((__aligned__(n)))
#define __always_inline         inline __attribute__((__always_inline__))
#define __noreturn              __attribute__((__noreturn__))
#define __used                  __attribute__((__used__))
#define __returns_twice         __attribute__((__returns_twice__))
#define __vector_size(n)        __attribute__((__vector_size__(n)))
#define __noinline              __attribute__((__noinline__))
#define __assume_aligned(n)     __attribute__((__assume_aligned__(n)))
#define __printf_format(m,n)    __attribute__((__format__(__printf__, m, n)))

#define __constructor(prio)     __attribute__((__constructor__ (prio)))
#define __destructor(prio)      __attribute__((__destructor__ (prio)))

#define __section(name)         __attribute__((__section__(name)))

#define CONCATENATE4(a, b) a##b
#define CONCATENATE3(a, b) CONCATENATE4(a, b)
#define CONCATENATE2(a, b) CONCATENATE3(a, b)
#define CONCATENATE(a, b) CONCATENATE2(a, b)

#ifdef __efi
typedef char16_t tchar;
#define TSTR u""
#else
typedef char tchar;
#define TSTR
#endif

#define countof(arr) (sizeof((arr))/sizeof(*(arr)))

