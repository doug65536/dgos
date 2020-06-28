#include <stdio.h>
#include <sys/likely.h>
#include "bits/cfile.h"

size_t fread(void *restrict buffer, size_t size,
             size_t count, FILE *restrict stream)
{
    if (unlikely(!stream))
        return 0;

    return stream->readbuf(buffer, size, count);
}
