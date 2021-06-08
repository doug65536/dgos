#include <string.h>

static __thread char *strtok_current;

char *strtok(char * restrict str, char const * restrict delim)
{
    return strtok_r(str, delim, &strtok_current);
}
