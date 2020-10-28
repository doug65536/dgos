#include <stdio.h>
#include "bits/cfile.h"

int fflush(FILE *stream)
{
    return stream->flush();
}
