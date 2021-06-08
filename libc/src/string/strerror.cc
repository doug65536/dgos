#include <string.h>

static __thread char strerror_message[80];

char *strerror(int err)
{
    if (strerror_r(err, strerror_message, sizeof(strerror_message)))
        return nullptr;
    return strerror_message;
}
