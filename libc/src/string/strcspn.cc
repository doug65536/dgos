#include <string.h>
#include <stdint.h>
#include "__bytebitmap.h"

size_t strcspn(char const *lhs, char const *rhs)
{
    // Build a bitmap of characters
    uint32_t bitmap[256 >> 5];
    __byte_bitmap(bitmap, rhs);

    // Count leading characters that are not in the bitmap
    size_t i, n;
    for (i = 0; (n = (unsigned char)lhs[i]) != 0 &&
         !(bitmap[n >> 5] & (uint32_t(1) << (n & 31))); ++i);

    return i;
}
