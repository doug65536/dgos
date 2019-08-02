#pragma once
#include <stdlib.h>
#include <sys/types.h>

struct _FILE {
    // Pointer to the read/write buffer
    char *buffer = nullptr;

    // File pointer at beginning of buffer
    off_t buf_pos = 0;

    // Current place in the file
    // Index into buffer will be seek_pos - buf_pos
    off_t seek_pos = 0;

    // Track fd seek position to attempt to elide seeks
    off_t fd_seek_pos = 0;

    // Amount of data in the buffer
    off_t buf_got = 0;

    // Size of the buffer
    off_t buf_sz = 0;

    // Half open range of bytes in buffer that have been modified
    int64_t dirty_pos = 0;
    int64_t dirty_end_pos = 0;

    int error = 0;

    // File handle (down here because it is a smaller type)
    int fd = -1;

    // Holds the ungetch value, or -1 if empty
    int unget = -1;

    // Whether the buffer is owned (and subsequently freed) by the libc
    bool buf_owned = false;

    int open(int new_fd);

    // Add a character to the buffer
    int add_ch(int ch);

    long readbuf(void *buffer, size_t size, size_t count);

    long writebuf(void const *buffer, size_t size, size_t count);

    long read_buffer_at(off_t ofs);

    long ensure_seek_pos(off_t pos);
};
