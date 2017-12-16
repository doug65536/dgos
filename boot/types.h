#pragma once

#include <stdint.h>
#include <stddef.h>

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
