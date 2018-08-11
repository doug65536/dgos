#pragma once

#define __ERRNO_DIRECT_TLS  1

typedef int errno_t;

#if __ERRNO_DIRECT_TLS
extern __thread errno_t errno;
#else

//extern "C" int *__errno_location();
//#define errno (*__errno_location())

#endif
