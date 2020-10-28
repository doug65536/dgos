#include <stdio.h>
#include "bits/cfile.h"

int fileno(FILE *stream)
{
    return stream->fd;
}
