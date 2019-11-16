#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/likely.h>
#include "bits/cfile.h"

char *fgets(char * restrict str, int count, FILE * restrict s)
{
    off_t took = 0;

    while (took < count - 1) {
        // If we have any data in the buffer to use, use it
        if (s->seek_pos >= s->buf_pos &&
                s->seek_pos < s->buf_pos + s->buf_got) {
            // Copy characters from the buffer, including newline, and
            // break after copying newline,
            // or,
            // break when there is only room for a null terminator
            // after writing the null terminator to output
            off_t idx = s->seek_pos - s->buf_pos;

            while (idx < s->buf_got) {
                ++idx;

                char ch = s->buffer[idx];
                str[took++] = ch;

                if (ch == '\n') {
                    // When we get newline, adjust count down to this point
                    count = idx + 1;
                    break;
                }

                if (took + 1 == count)
                    break;
            }
            s->seek_pos += took;

            str[took] = 0;
        } else {
            // Replenish buffer

            if (!s->buffer) {
                s->buf_sz = 4096;
                s->buffer = (char*)malloc(s->buf_sz);
                s->buf_owned = true;

                if (unlikely(!s->buffer)) {
                    s->error = ENOMEM; return nullptr;
                }
            }

            // Update the seek position if necessary
            if (s->fd_seek_pos != s->seek_pos) {
                s->fd_seek_pos = lseek(s->fd, s->seek_pos, SEEK_SET);
                s->seek_pos = s->fd_seek_pos;
            }
            size_t got = read(s->fd, s->buffer, BUFSIZ);

            if (got > 0) {
                s->buf_pos = s->seek_pos;
                s->fd_seek_pos += got;
            }
        }
    }

    return nullptr;
}
