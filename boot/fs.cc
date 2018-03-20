#include "fs.h"

fs_api_t fs_api;

int boot_open(char const *filename)
{
    return fs_api.boot_open(filename);
}

int boot_close(int file)
{
    return fs_api.boot_close(file);
}

ssize_t boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    return fs_api.boot_pread(file, buf, bytes, ofs);
}

uint64_t boot_serial()
{
    return fs_api.boot_drv_serial();
}
