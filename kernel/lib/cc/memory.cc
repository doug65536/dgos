#include "memory.h"
#include "mm.h"


void *page_allocator_base::allocate_impl(size_t n)
{
    return mmap(nullptr, __n * sizeof(value_type),
                PROT_READ | PROT_WRITE, 0, -1, 0);
}

void page_allocator_base::deallocate_impl(void *p, size_t n)
{
    munmap(__p, __n);
}
