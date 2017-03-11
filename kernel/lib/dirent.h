#pragma once
#include "types.h"

typedef uint64_t ino_t;
typedef uint16_t mode_t;

typedef struct dirent_t {
    ino_t d_ino;
    char     d_name[256];
} dirent_t;
