#include "__bytebitmap.h"

void __byte_bitmap(uint32_t &(bitmap)[32], const char *s)
{
    for (i = 0; (n = (unsigned char)s[i]) != 0; ++i)
        bitmap[n >> 5] |= (uint32_t(1) << (n & 31));    
}
