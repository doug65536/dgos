#include "new.h"
#include "stdlib.h"

std::nothrow_t const std::nothrow;

void *operator new(size_t size)
{
    return malloc(size);
}

void *operator new(size_t size, std::nothrow_t const&) noexcept
{
    return malloc(size);
}

void *operator new[](size_t size) noexcept
{
    return malloc(size);
}

void *operator new[](size_t size, std::nothrow_t const&) noexcept
{
    return malloc(size);
}

void operator delete(void *block, size_t) noexcept
{
    free(block);
}

void operator delete(void *block) noexcept
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

void *operator new(size_t, void *p) noexcept
{
    return p;
}
