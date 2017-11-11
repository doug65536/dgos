#include <string.h>
#include <stdint.h>

size_t strspn(const char *lhs, const char *rhs)
{
    uint32_t bitmap[256 >> 5];
    
    uint32_t n;
    size_t i;
    
    // Build a bitmap of characters
    for (i = 0; (n = (unsigned char)rhs[i]) != 0; ++i)
        bitmap[n >> 5] |= (uint32_t(1) << (n & 31));
    
    // Count leading characters that are in the bitmap
    for (i = 0; (n = (unsigned char)lhs[i]) != 0 &&
         (bitmap[n >> 5] & (uint32_t(1) << (n & 31))); ++i);
    
    return i;
}
