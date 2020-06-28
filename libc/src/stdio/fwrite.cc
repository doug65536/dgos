#include <stdio.h>
#include <sys/likely.h>
#include <errno.h>
#include "bits/cfile.h"

size_t fwrite(void const *restrict buffer, size_t size,
              size_t count, FILE *restrict stream)
{
    if (unlikely(!(size | count)))
        return 0;

    return stream->writebuf(buffer, size, count);
}
