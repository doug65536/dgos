#include <stdio.h>
#include <string.h>
#include <sys/likely.h>

int puts(char const *str)
{
    size_t len = strlen(str);

    if (unlikely(fwrite(str, 1, len, stdout) != len))
        return -1;

    if (unlikely(fputc('\n', stdout) < 0))
        return -1;

    return len + 1;
}
