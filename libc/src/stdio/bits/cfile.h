#pragma once
#include <stdlib.h>
#include <sys/types.h>

struct _FILE {
    // Pointer to the read/write buffer
    char *buffer;

    // File pointer at beginning of buffer
    off_t buf_pos;

    // Current place in the file
    // Index into buffer will be seek_pos - buf_pos
    off_t seek_pos;

    // Track fd seek position to attempt to elide seeks
    off_t fd_seek_pos;

    // Amount of data in the buffer
    off_t buf_got;

    // Size of the buffer
    off_t buf_sz;

    // Half open range of bytes in buffer that have been modified
    int64_t dirty_pos;
    int64_t dirty_end_pos;

    // File handle (down here because it is a smaller type)
    int fd;

    // Holds the ungetch value, or -1 if empty
    int unget;

    // Whether the buffer is owned (and subsequently freed) by the libc
    bool buf_owned;

    int open(int new_fd);

    // Add a character to the buffer
    int add_ch(int ch);

    long readbuf(void *buffer, size_t size, size_t count);

    long writebuf(void const *buffer, size_t size, size_t count);

    long read_buffer_at(off_t ofs);

    long ensure_seek_pos(off_t pos);
};
