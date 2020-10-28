#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/likely.h>
#include "bits/cfile.h"

int fseek(FILE *stream, off_t offset, int origin)
{
    if (unlikely(!stream || origin < 0 || origin > SEEK_END)) {
        errno = EINVAL;
        return -1;
    }

    // fseek undoes the effect of ungetc
    stream->unget_pos = _FILE::unget_size;

    // POSIX states that fseek should flush writes
    if (stream->dirty_pos != stream->dirty_end_pos) {
        off_t write_sz = stream->dirty_end_pos - stream->dirty_pos;

        char const *write_src = stream->buffer +
                stream->dirty_pos - stream->buf_pos;

        int written = write(stream->fd, write_src, write_sz);

        // Fail the seek if the write failed
        // Also, leave the dirty state alone so retry can work
        if (unlikely(written != write_sz))
            return -1;
    }
    
    return 0;
}
