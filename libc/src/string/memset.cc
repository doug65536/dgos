#include <string.h>

void *memset(void *lhs, int c, size_t sz)
{
    auto d = (char *)lhs;
    auto cc = char(c);
    
    for (size_t i = 0; i < sz; ++i)
        d[i] = cc;
    
    return d;
}
