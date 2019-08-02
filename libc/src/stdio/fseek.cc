#include <stdio.h>
#include <unistd.h>
#include "bits/cfile.h"

int fseek(FILE *stream, off_t offset, int origin)
{
    // fseek undoes the effect of ungetc
    stream->unget = -1;

    // POSIX states that fseek should flush writes
    if (stream->dirty_pos != stream->dirty_end_pos) {
        off_t write_sz = stream->dirty_end_pos - stream->dirty_pos;

        char const *write_src = stream->buffer +
                stream->dirty_pos - stream->buf_pos;

        int written = write(stream->fd, write_src, write_sz);

        // Fail the seek if the write failed
        // Also, leave the dirty state alone so retry can work
        if (written != write_sz)
            return -1;
    }
}
