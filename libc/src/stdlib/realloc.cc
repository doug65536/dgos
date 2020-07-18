#include <stdlib.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/likely.h>

void *realloc(void *mem, size_t new_size)
{
    size_t *real_ptr = (size_t*)((char*)mem - 64);
    size_t old_size = ((*real_ptr + 64) + 4096) & -4096;

    void *new_mem = mremap(real_ptr, old_size,
                           new_size + 64, MREMAP_MAYMOVE);

    if (unlikely(new_mem == MAP_FAILED)) {
        errno = ENOMEM;
        return nullptr;
    }

    real_ptr = (size_t*)new_mem;
    *real_ptr = new_size;

    return (char*)new_mem + 64;
}
