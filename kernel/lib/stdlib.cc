#include "stdlib.h"
#include "string.h"
#include "mm.h"
#include "assert.h"
#include "printk.h"
#include "heap.h"
#include "callout.h"
#include "thread.h"
#include "cpu/atomic.h"
#include "export.h"

std::nothrow_t const std::nothrow;

// Always use paged allocation with guard pages
// Realloc always moves the memory to a new range
#define HEAP_PAGEONLY 0

#if !HEAP_PAGEONLY
static heap_t **default_heaps;
static size_t heap_count;

void malloc_startup(void*)
{
    // Create a heap
    heap_t *default_heap = heap_create();

    // Allocate an array for per-cpu heap pointers using the BSP heap
    default_heaps = (heap_t**)heap_alloc(
                default_heap, sizeof(*default_heaps) * 1);

    if (unlikely(!default_heaps))
        panic_oom();

    // Place the BSP heap pointer as the CPU0 heap
    default_heaps[0] = default_heap;

    // Announce that we have a working heap
    callout_call(callout_type_t::heap_ready);
}

static void malloc_statup_smp(void*)
{
    // Lock free transition to N per-cpu heaps
    size_t new_heap_count = thread_get_cpu_count();

    heap_t **new_default_heaps = new (std::nothrow) heap_t *[new_heap_count];
    if (unlikely(!default_heaps))
        panic_oom();

    // Bring in the uniprocessor heap as the CPU 0 heap entry
    new_default_heaps[0] = default_heaps[0];

    for (size_t i = 1; i < new_heap_count; ++i)
    {
        new_default_heaps[i] = heap_create();


        if (unlikely(!new_default_heaps[i]))
            panic_oom();

        // Make sure the first N cpu-local heaps get heap ID (0)thru(N-1)
        assert(heap_get_heap_id(new_default_heaps[i]) == i);
    }

    auto old_default_heaps = atomic_xchg(&default_heaps, new_default_heaps);
    atomic_st_rel(&heap_count, new_heap_count);

    delete[] old_default_heaps;
}

REGISTER_CALLOUT(malloc_statup_smp, nullptr, callout_type_t::smp_online, "000");

static heap_t *this_cpu_heap()
{
    auto cpu = thread_cpu_number();
    assert(heap_count == 0 || size_t(cpu) < heap_count);
    return default_heaps[cpu];
}

static heap_t *heap_get_block_heap(void *block)
{
    int id = heap_get_block_heap_id(block);
    assert(heap_count == 0 || size_t(id) < heap_count);
    return default_heaps[id];
}

EXPORT void *calloc(size_t num, size_t size)
{
    return heap_calloc(this_cpu_heap(), num, size);
}

EXPORT void *malloc(size_t size)
{
    return heap_alloc(this_cpu_heap(), size);
}

EXPORT void *realloc(void *p, size_t new_size)
{
    return heap_realloc(heap_get_block_heap(p), p, new_size);
}

EXPORT void free(void *p)
{
    heap_free(heap_get_block_heap(p), p);
}
#else

void malloc_startup(void *p)
{
}

EXPORT void *calloc(size_t num, size_t size)
{
    return pageheap_calloc(num, size);
}

EXPORT void *malloc(size_t size)
{
    return pageheap_alloc(size);
}

EXPORT void *realloc(void *p, size_t new_size)
{
    return pageheap_realloc(p, new_size);
}

EXPORT void free(void *p)
{
    pageheap_free(p);
}

#endif

EXPORT char *strdup(char const *s)
{
    size_t len = strlen(s);
    char *b = new (std::nothrow) char[len+1];
    return (char*)memcpy(b, s, len+1);
}

// banned throwing new, linker error please
//EXPORT void *operator new(size_t size)
//{
//    return malloc(size);
//}

EXPORT void *operator new(size_t size, std::nothrow_t const&) noexcept
{
    return malloc(size);
}

// banned throwing new, linker error please
//EXPORT void *operator new[](size_t size)
//{
//    return malloc(size);
//}

EXPORT void *operator new[](size_t size, std::nothrow_t const&) noexcept
{
    return malloc(size);
}

EXPORT void operator delete(void *block, size_t) noexcept
{
    free(block);
}

EXPORT void operator delete(void *block) noexcept
{
    free(block);
}

EXPORT void operator delete[](void *block) noexcept
{
    free(block);
}

EXPORT void operator delete[](void *block, size_t) noexcept
{
    free(block);
}

EXPORT void *operator new(size_t, void *p) noexcept
{
    return p;
}

// FIXME: Overflow is not handled properly
template<typename T>
static T strto(char const *str, char **end, int base)
{
    T n = 0;
    T digit;
    T sign = 0;

    for (;; ++str) {
        char c = *str;

        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'Z')
            digit = c + 10 - 'A';
        else if (c >= 'a' && c <= 'z')
            digit = c + 10 - 'a';
        else if (c == '-' && sign == 0) {
            sign = -1;
            continue;
        } else if (c == '+' && sign == 0) {
            sign = 1;
            continue;
        }
        else
            break;

        if (digit >= base)
            break;

        ++str;
        n *= base;
        n += digit;
    }

    if (end)
        *end = (char*)str;

    return !sign ? n : n * sign;
}

EXPORT int strtoi(char const *str, char **end, int base)
{
    return strto<int>(str, end, base);
}

EXPORT long strtol(char const *str, char **end, int base)
{
    return strto<long>(str, end, base);
}

EXPORT long long strtoll(char const *str, char **end, int base)
{
    return strto<long long>(str, end, base);
}
