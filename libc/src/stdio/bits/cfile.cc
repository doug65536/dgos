#include "cfile.h"
#include <stdio.h>
#include <unistd.h>

int _FILE::open(int fd)
{
    this->fd = fd;
    return 0;
}

int _FILE::add_ch(int ch)
{
    write(fd, &ch, 1);
    return 0;
}

long _FILE::readbuf(void *buffer, size_t size, size_t count)
{
    if (unget >= 0) {
        *((char*)buffer) = unget;
        buffer = (char*)buffer + 1;
        unget = -1;
        --size;
    }
    return read(fd, buffer, size * count);
}

long _FILE::writebuf(const void *buffer, size_t size, size_t count)
{
    return write(fd, buffer, size * count);
}
