#pragma once

#define _avx2  __attribute__((__target__("avx2")))
#define _sse4_1  __attribute__((__target__("sse4.1")))
#define _ssse3  __attribute__((__target__("ssse3")))

int start_framebuffer();
