#include <stdio.h>
#include "bits/cfile.h"

int ferror(FILE *stream)
{
    return stream->error == 0 ? 0 : -1;
}
