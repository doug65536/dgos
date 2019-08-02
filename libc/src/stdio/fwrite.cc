#include <stdio.h>
#include "bits/cfile.h"

size_t fwrite(void const *restrict buffer, size_t size,
              size_t count, FILE *restrict stream)
{
    return stream->writebuf(buffer, size, count);
}
