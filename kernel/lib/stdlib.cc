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

ext::nothrow_t const ext::nothrow;

// Always use paged allocation with guard pages
// Realloc always moves the memory to a new range
#define HEAP_PAGEONLY 0

static heap_t **default_heaps;

#if !HEAP_PAGEONLY
static size_t heap_count;

void malloc_startup(void*)
{
    // Create a heap
    heap_t *default_heap = heap_create(512);

    // Allocate an array for per-cpu heap pointers using the BSP heap
    default_heaps = (heap_t**)heap_alloc(
                default_heap, 1 * sizeof(*default_heaps));

    // Place the BSP heap pointer as the CPU0 heap
    default_heaps[0] = default_heap;

    heap_count = 1;

    if (unlikely(!default_heaps))
        panic_oom();

    // Announce that we have a working heap
    callout_call(callout_type_t::heap_ready);
}

static void malloc_startup_smp(void*)
{
    // Lock free transition to N per-cpu heaps
    size_t new_heap_count = thread_get_cpu_count();

    heap_t **new_default_heaps = new (ext::nothrow) heap_t *[new_heap_count]();
    if (unlikely(!default_heaps))
        panic_oom();

    // Bring in the uniprocessor heap as the CPU 0 heap entry
    new_default_heaps[0] = default_heaps[0];

    for (size_t i = 1; i < new_heap_count; ++i)
    {
        new_default_heaps[i] = heap_create(512);

        if (unlikely(!new_default_heaps[i]))
            panic_oom();

        // Make sure the first N cpu-local heaps get heap ID (0)thru(N-1)
        assert(heap_get_heap_id(new_default_heaps[i]) == i);
    }

    auto old_default_heaps = atomic_xchg(&default_heaps, new_default_heaps);
    atomic_st_rel(&heap_count, new_heap_count);

    heap_free(default_heaps[0], old_default_heaps);
}

REGISTER_CALLOUT(malloc_startup_smp, nullptr,
                 callout_type_t::smp_online, "000");

static heap_t *this_cpu_heap()
{
    auto cpu = thread_cpu_number();
    assert(heap_count == 0 || size_t(cpu) < heap_count);
    return likely(default_heaps) ? default_heaps[cpu] : nullptr;
}

static heap_t *heap_get_block_heap(void *block)
{
    int id = heap_get_block_heap_id(block);
    assert(heap_count == 0 || size_t(id) < heap_count);
    return default_heaps[id];
}

void *calloc(size_t num, size_t size)
{
    return heap_calloc(this_cpu_heap(), num, size);
}

void *malloc(size_t size)
{
    return heap_alloc(this_cpu_heap(), size);
}

void *realloc(void *p, size_t new_size)
{
    return heap_realloc(heap_get_block_heap(p), p, new_size);
}

void free(void *p)
{
    heap_free(heap_get_block_heap(p), p);
}

bool malloc_validate(bool dump)
{
    return heap_validate(this_cpu_heap(), dump);
}
#else

bool malloc_validate(bool dump)
{
    return true;//not supported
}

void malloc_startup(void *p)
{
    // Create a heap
    heap_t *default_heap = pageheap_create();

    // Allocate an array for per-cpu heap pointers using the BSP heap
    default_heaps = (heap_t**)pageheap_alloc(
                default_heap, sizeof(*default_heaps) * 1);

    if (unlikely(!default_heaps))
        panic_oom();

    // Place the BSP heap pointer as the CPU0 heap
    default_heaps[0] = default_heap;

    // Announce that we have a working heap
    callout_call(callout_type_t::heap_ready);
}

void *calloc(size_t num, size_t size)
{
    return pageheap_calloc(default_heaps[0], num, size);
}

void *malloc(size_t size)
{
    return pageheap_alloc(default_heaps[0], size);
}

void *realloc(void *p, size_t new_size)
{
    return pageheap_realloc(default_heaps[0], p, new_size);
}

void free(void *p)
{
    pageheap_free(default_heaps[0], p);
}

#endif

char *strdup(char const *s)
{
    size_t len = strlen(s);
    char *b = (char*)malloc(len+1);
    return b ? (char*)memcpy(b, s, len+1) : nullptr;
}

// banned throwing new, linker error please
//void *operator new(size_t size)
//{
//    return malloc(size);
//}

void *operator new(size_t size, ext::nothrow_t const&) noexcept
{
    return malloc(size);
}

// banned throwing new, linker error please
//EXPORT void *operator new[](size_t size)
//{
//    return malloc(size);
//}

void *operator new[](size_t size, ext::nothrow_t const&) noexcept
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

// FIXME: Overflow is not handled properly
template<typename T>
static T strto(char const *str, char **end, int base)
{
    T n = 0;
    T digit;
    T sign = 0;

    for (;; ++str) {
        char c = *str;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'Z') {
            digit = c + 10 - 'A';
        } else if (c >= 'a' && c <= 'z') {
            digit = c + 10 - 'a';
        } else if (c == '-' && sign == 0) {
            sign = -1;
            continue;
        } else if (c == '+' && sign == 0) {
            sign = 1;
            continue;
        } else {
            break;
        }

        if (unsigned(digit) >= unsigned(base))
            break;

        n *= base;
        n += digit;
    }

    if (end)
        *end = (char*)str;

    return !sign ? n : n * sign;
}

int strtoi(char const *str, char **end, int base)
{
    return strto<int>(str, end, base);
}

long strtol(char const *str, char **end, int base)
{
    return strto<long>(str, end, base);
}

unsigned long strtoul(char const *str, char **end, int base)
{
    return strto<unsigned long>(str, end, base);
}

long long strtoll(char const *str, char **end, int base)
{
    return strto<long long>(str, end, base);
}

unsigned long long strtoull(char const *str, char **end, int base)
{
    return strto<unsigned long long>(str, end, base);
}
