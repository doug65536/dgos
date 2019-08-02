#include "__bytebitmap.h"
#include <stdint.h>
#include <stddef.h>

void __byte_bitmap(uint32_t (&bitmap)[256>>5], char const *s)
{
    unsigned char n;
    for (size_t i = 0; (n = (unsigned char)s[i]) != 0; ++i)
        bitmap[n >> 5] |= (uint32_t(1) << (n & 31));
}
