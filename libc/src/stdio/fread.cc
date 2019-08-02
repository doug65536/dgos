#include <stdio.h>
#include "bits/cfile.h"

size_t fread(void *restrict buffer, size_t size,
             size_t count, FILE *restrict stream)
{
    return stream->readbuf(buffer, size, count);
}
