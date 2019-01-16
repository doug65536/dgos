#include <stdio.h>
#include "bits/cfile.h"
#include <stdlib.h>
#include <new.h>

FILE *fdopen(int fd, char const *mode)
{
    FILE *f = new FILE{};
    f->fd = fd;
    return f;
}
