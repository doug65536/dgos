#include <stdio.h>
#include <unistd.h>
#include <sys/likely.h>

int putchar(int ch)
{
    unsigned char c = (unsigned char)ch;

    int wrote = write(STDOUT_FILENO, &c, sizeof(c));

    if (unlikely(wrote < 0))
        return -1;

    return 0;
}
