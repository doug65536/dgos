#include <stdio.h>
#include <unistd.h>
#include <sys/likely.h>
#include "bits/cfile.h"

int fclose(FILE *stream)
{
    return stream->close();
}
