#include <errno.h>

#if __ERRNO_DIRECT_TLS
__thread errno_t errno;
#else
__thread int errno_value;

int *__errno_location()
{
    return &errno_value;
}
#endif
