#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

void free(void *p)
{
    p = (void*)(uintptr_t(p) & -(4 << 10));
    size_t sz = *(size_t*)p;
    munmap(p, sz+64);
}
