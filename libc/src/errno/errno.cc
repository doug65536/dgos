#include <errno.h>

__thread int errno_value;

int *__errno_location()
{
    return &errno_value;
}
