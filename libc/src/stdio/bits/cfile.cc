#include "cfile.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/likely.h>

int _FILE::open(int new_fd)
{
    fd = new_fd;
    return 0;
}

int _FILE::flush()
{
    return 0;
}

int _FILE::close()
{
    if (unlikely(fflush(this)))
        return -1;
    
    if (unlikely(::close(fd) < 0))
        return -1;
    
    fd = -1;
    
    delete this;
    
    return 0;
}

int _FILE::add_ch(int ch)
{
    unsigned char uc = ch;
    
    ssize_t sz = write(fd, &uc, 1);
    
    if (unlikely(sz < 0))
        return -1;

    return 0;
}

long _FILE::readbuf(void *buffer, size_t size, size_t count)
{
    size_t bytes = size * count;

    if (unlikely(bytes == 0))
        return 0;

    long result = EOF;

    int ungot = 0;

    size_t ungot_available = unget_size - unget_pos;

    if (unlikely(ungot_available > 0)) {
        size_t take = ungot_available > count
                ? count
                : ungot_available;

        memcpy(buffer, unget_data, take);

        unget_pos += take;

        result = take;
    }

    if (bytes > 0) {
        ensure_seek_pos(seek_pos);
        long read_result = read(fd, buffer, bytes);

        if (read_result > 0) {
            fd_seek_pos += read_result;
            return ungot + bytes;
        }
    }

    return result;
}

long _FILE::writebuf(void const *buffer, size_t size, size_t count)
{
    return write(fd, buffer, size * count);
}

long _FILE::ensure_seek_pos(off_t pos)
{
    if (fd_seek_pos != pos) {
        int seek_result = lseek(fd, pos, SEEK_SET);
        
        if (seek_result != pos)
            return EOF;
            
        fd_seek_pos = pos;
    }
    return pos;
}
