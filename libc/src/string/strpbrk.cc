#include <string.h>
#include <stdint.h>
#include "__bytebitmap.h"

char *strpbrk(const char *s, const char *b)
{
    uint32_t bitmap[256 >> 5];
    __byte_bitmap(bitmap, b);
    
    uint32_t n;
    for (size_t i = 0; (n = (unsigned char)s[i]) != 0; ++i) {
        if (bitmap[n >> 5] & (uint32_t(1) << (n & 31)))
            return (char*)(s + i);
    }
    return nullptr;
}
