#include "new.h"
#include "stdlib.h"

void *operator new(size_t size)
{
    return malloc(size);
}

void *operator new[](size_t size)
{
    return malloc(size);
}

void operator delete(void *block, size_t)
{
    free(block);
}

void operator delete(void *block) throw()
{
    free(block);
}

void operator delete[](void *block) noexcept
{
    free(block);
}

void operator delete[](void *block, size_t) noexcept
{
    free(block);
}

__const
void *operator new(size_t, void *p) noexcept
{
    return p;
}
