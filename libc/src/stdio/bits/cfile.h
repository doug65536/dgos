#pragma once
#include <stdlib.h>

struct _FILE {
    int fd;

    // Pointer to the read/write buffer
    char *buffer;

    // Pointer to the next character to read / next place to store write
    size_t buf_ofs;

    // Amount of data in the buffer
    size_t buf_got;

    // Size of the buffer
    size_t buf_sz;

    // Whether the buffer is owned (and subsequently freed) by the libc
    bool buf_owned;

    // Holds the ungetch value, or -1 if empty
    int unget;

    int open(int fd);

    // Add a character to the buffer
    int add_ch(int ch);

    long readbuf(void *buffer, size_t size, size_t count);

    long writebuf(void const *buffer, size_t size, size_t count);
};
