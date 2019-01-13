#include "cfile.h"
#include <stdio.h>
#include <unistd.h>

int _FILE::open(int new_fd)
{
    fd = new_fd;
    return 0;
}

int _FILE::add_ch(int ch)
{
    write(fd, &ch, 1);
    return 0;
}

long _FILE::readbuf(void *buffer, size_t size, size_t count)
{
    size_t bytes = size * count;

    if (bytes == 0)
        return 0;

    int ungot = 0;

    if (unget >= 0) {
        *((char*)buffer) = unget;
        buffer = (char*)buffer + 1;
        unget = -1;
        --bytes;
        ungot = 1;
    }

    if (bytes > 0) {
        ensure_seek_pos(seek_pos);
        long read_result = read(fd, buffer, bytes);

        if (read_result > 0) {
            fd_seek_pos += read_result;
            return ungot + bytes;
        }
    }

    return 0;
}

long _FILE::writebuf(const void *buffer, size_t size, size_t count)
{
    return write(fd, buffer, size * count);
}

long _FILE::ensure_seek_pos(off_t pos)
{
    if (fd_seek_pos != pos) {
        if (lseek(fd, pos, SEEK_SET) != pos)
            return EOF;
        fd_seek_pos = pos;
    }
    return pos;
}
