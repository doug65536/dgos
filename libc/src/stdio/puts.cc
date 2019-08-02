#include <stdio.h>
#include <string.h>
#include <unistd.h>

int puts(char const *str)
{
    size_t len = strlen(str);
    if (fwrite(str, 1, len, stdout) != len)
        return -1;
    if (fputc('\n', stdout) < 0)
        return -1;
    return len + 1;
}
