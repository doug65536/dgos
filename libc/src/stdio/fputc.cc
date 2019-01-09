#include <stdio.h>
#include "bits/cfile.h"

int fputc(int ch, FILE *stream)
{
    return stream->add_ch(ch);
}
