#include <stdlib.h>
#include <string.h>

void *calloc(size_t n, size_t k)
{
    n *= k;
    void *m = malloc(n);
    if (m)
        return memset(m, 0, n);
    return nullptr;
}
