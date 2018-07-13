#include <string.h>
#include <stdint.h>
#include "__bytebitmap.h"

size_t strspn(char const *lhs, char const *rhs)
{
    uint32_t bitmap[256 >> 5];
    __byte_bitmap(bitmap, rhs);

    uint32_t n;
    size_t i;
    for (i = 0; (n = (unsigned char)lhs[i]) != 0 &&
         (bitmap[n >> 5] & (uint32_t(1) << (n & 31))); ++i);

    return i;
}
