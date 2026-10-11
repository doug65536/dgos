#include "testassert.h"
#include <stdio.h>
#include <sys/syscall.h>

static bool case_failed;
static int case_pass;

static testcase_t::test_fn test_cases[1024];
static size_t test_cases_count;

void testcase_t::add(test_fn test)
{
    test_cases[test_cases_count++] = test;
}

int main()
{
    for (size_t i = 0; i < test_cases_count; ++i) {
        case_failed = false;
        case_pass = false;

        test_cases[i]();
    }

    return 0;
}

extern void (*__init_array_start[])();
extern void (*__init_array_end[])();

extern void (*__init_array_early_start[])() __attribute__((weak));
extern void (*__init_array_early_end[])() __attribute__((weak));

extern "C" {
extern char const __rela_iplt_start[] __attribute__((weak));
extern char const __rela_iplt_end[] __attribute__((weak));
}

extern "C" void _start()
{
    // Static binary: apply IRELATIVE relocations for ifunc'd functions
    struct rela_t {
        uintptr_t offset;
        uintptr_t info;
        intptr_t addend;
    };

    for (rela_t const *r = (rela_t const *)__rela_iplt_start;
         r < (rela_t const *)__rela_iplt_end; ++r) {
        auto resolver = (uintptr_t (*)())r->addend;
        *(uintptr_t*)r->offset = resolver();
    }

    size_t n = __init_array_early_end - __init_array_early_start;

    for (size_t i = 0; i < n; ++i)
        if (__init_array_early_start[i])
            __init_array_early_start[i]();

    n = __init_array_end - __init_array_start;

    for (size_t i = 0; i < n; ++i)
        if (__init_array_start[i])
            __init_array_start[i]();

    //__do_global_ctors_aux();
    main();

    __asm__ __volatile__ ( "syscall" : : "a" (60), "D" (0) );
}
