#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
//#include <string.h>
//#include <type_traits>
//#include <algorithm>
//#include <string>
//#include <vector>
//#include <memory>

#include "termesc.h"
static constexpr char const *term_red = TERMESC_RED;
static constexpr char const *term_green = TERMESC_GREEN;
static constexpr char const *term_blue = TERMESC_BLUE;
static constexpr char const *term_white = TERMESC_WHITE;

class testcase_t {
public:
    using test_fn = void (*)();
    
    testcase_t(test_fn test)
    {
        add(test);
    }
    
    static void add(test_fn test);
};

void output_pack(intptr_t v, bool in_hex = false);
void output_pack(uintptr_t v, bool in_hex = false);
void output_pack(char const *s);

template<typename T>
void output_pack(T *v)
{
    output_pack(uintptr_t(v), true);
}

template<typename T1, typename... Rest>
void output_pack(T1&& first, Rest&& ...rest)
{
    output_pack(first);
    output_pack(rest...);
}

template<typename... Args>
void report_failure(Args&& ...args)
{
    output_pack(term_red, "[ FAILED ]", term_white, " ");
    output_pack(args...);
}

template<typename... Args>
void report_success(Args&& ...args)
{
    output_pack(term_green, "[ PASSED ]", term_white, " ");
    output_pack(args...);
}

template<typename TE, typename TA, size_t sz>
bool compare(TE (&expect)[sz], TA (&actual)[sz])
{
    for (size_t i = 0; i < sz; ++i)
        if (expect[i] != actual[i])
            return false;
    return true;
}

static __always_inline bool compare(char const *expect, char const *actual)
{
    return !strcmp(expect, actual);
}

template<typename TE, typename TA, size_t sz>
bool compare(TE expect, TA actual)
{
    return expect == actual;
}

template<typename TE, typename TA>
void are_eq(TE const& expect,
            TA const& actual, 
            char const* expect_expr, 
            char const* actual_expr,
            char const* description,
            char const* func,
            char const* file, int line)
{
    bool passed = compare(expect, actual);
    if (passed) {
        report_failure("Expected ", expect_expr, 
                       ", got ", actual_expr, 
                       ": ", description,
                       " in ", func,
                       " at ", file, "(", line, ")");
    } else {
        report_success(description);
    }
}

#define TEST_CASE(name) \
    void test_##name(); \
    testcase_t test_register_##name(test_##name); \
    void test_##name()
