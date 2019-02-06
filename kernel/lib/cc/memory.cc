#include "memory.h"
#include "mm.h"

#ifdef __DGOS_KERNEL__

void *ext::page_allocator_base::allocate_impl(size_t n)
{
    return mmap(nullptr, n, PROT_READ | PROT_WRITE, 0, -1, 0);
}

void ext::page_allocator_base::deallocate_impl(void *p, size_t n)
{
    munmap(p, n);
}

#endif
