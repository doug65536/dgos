#include <stdlib.h>
#include <string.h>
#include <sys/likely.h>

void *calloc(size_t n, size_t k)
{
    n *= k;
    void *m = malloc(n);
    if (likely(m))
        return memset(m, 0, n);
    return nullptr;
}
