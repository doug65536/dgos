#include "testassert.h"
#include <stdio.h>
#include <vector>
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

extern "C" void _start()
{
    size_t n = __init_array_end - __init_array_start;

    for (size_t i = 0; i < n; ++i)
        if (__init_array_start[i])
            __init_array_start[i]();

    //__do_global_ctors_aux();
    main();

    __asm__ __volatile__ ( "syscall" : : "a" (60), "D" (0) );
}
