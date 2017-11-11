#include "zlib_helper.h"
#include "zlib/zlib.h"
#include "string.h"
#include "stdlib.h"

void zlib_init(z_stream *strm)
{
    memset(strm, 0, sizeof(*strm));
    strm->zalloc = zlib_malloc;
    strm->zfree = zlib_free;
}

void *zlib_malloc(void *opaque, unsigned items, unsigned size)
{
    (void)opaque;
    return malloc(items * size);
}

void zlib_free(void *opaque, void *p)
{
    (void)opaque;
    free(p);
}
