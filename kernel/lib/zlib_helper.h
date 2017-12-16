#pragma once
#if 0
#include "zlib/zlib.h"

void zlib_init(z_stream *strm);
void *zlib_malloc(void *opaque, unsigned items, unsigned size);
void zlib_free(void *opaque, void *p);
#endif
