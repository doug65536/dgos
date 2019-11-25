#pragma once
#include "types.h"
#include "string.h"
#include "printk.h"
#include "likely.h"

namespace unittest {

class unit;

class unit_ctx {
public:
    void fail(unit *test, const char *file, int line);
    void skip(unit *test);

    size_t failure_count() const
    {
        return failures;
    }

    size_t skip_count() const
    {
        return failures;
    }

private:
    size_t failures = 0;
    size_t skipped = 0;
};

#ifdef __clang__
#define __builtin_FILE() 0
#define __builtin_LINE() 0
#endif

class unit {
public:
    unit(char const *name, char const *test_file, int test_line,
         bool init_enabled);

    _noinline
    void fail(const char *file, int line);

    void set_ctx(unit_ctx *ctx);

    bool enabled() const {
        return is_enabled;
    }

    void enabled(bool en) {
        is_enabled = en;
    }

    bool float_thread() const {
        return is_float_thread;
    }

    void float_thread(bool en) {
        is_float_thread = en;
    }

    virtual void invoke() = 0;

    void eq(char const *expect, char *value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void eq_np(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void eq(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void ne_np(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void ne(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void lt(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void gt(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void le(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void ge(T const& expect, U const& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    char const *get_name() const;
    char const *get_file() const;
    int get_line() const;

    static void run_all(unit_ctx *ctx);

protected:
    unit_ctx *ctx = nullptr;
    unit *next = nullptr;

    bool is_enabled;
    bool is_float_thread = false;

    static unit *list_st;
    static unit *list_en;

private:
    static int thread_fn(void *arg);

    char const * const name = nullptr;
    char const * const test_file = nullptr;
    int const test_line = 0;
};

template<typename T, typename U>
void unit::eq_np(T const& expect, U const& value,
        char const *file, int line)
{
    if (unlikely(!(expect == value))) {
        dbgout << name << " got wrong value\n";
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::eq(T const& expect, U const& value,
        char const *file, int line)
{
    if (unlikely(!(expect == value))) {
        dbgout << name << " expected \"" << expect << '"' <<
                  " but got " << value << '\n';
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::ne_np(T const& expect, U const& value,
              char const *file, int line)
{
    if (unlikely(!(expect != value))) {
        dbgout << name << " has unwanted value\n";
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::ne(T const& expect, U const& value,
              char const *file, int line)
{
    if (unlikely(!(expect != value))) {
        dbgout << name << " expected \"" << expect <<
                  " not equal to " << value << '\n';
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::lt(T const& expect, U const& value,
              char const *file, int line)
{
    if (unlikely(!(expect < value))) {
        dbgout << name << " expected \"" << expect <<
                  " to be less than " << value << '\n';
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::gt(T const& expect, U const& value,
              char const *file, int line)
{
    if (unlikely(!(expect > value))) {
        dbgout << name << " expected \"" << expect <<
                  " to be greater than " << value << '\n';
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::le(T const& expect, U const& value,
              char const *file, int line)
{
    if (unlikely(!(expect <= value))) {
        dbgout << name << " expected \"" << expect <<
                  " to be less than or equal to " << value << '\n';
        fail(file, line);
    }
}

template<typename T, typename U>
void unit::ge(T const& expect, U const& value,
              char const *file, int line)
{
    if (unlikely(!(expect >= value))) {
        dbgout << name << " expected \"" << expect <<
                  " to be greater than or equal to " << value << '\n';
        fail(file, line);
    }
}

extern template void unit::eq(int const&, int const&,
    const char *file, int line);
extern template void unit::eq(uint32_t const&, uint32_t const&,
    const char *file, int line);
extern template void unit::eq(bool const&, bool const&,
    const char *file, int line);
extern template void unit::eq(size_t const&, size_t const&,
    const char *file, int line);

#define UNITTEST_CONCAT2(a,b) a##b
#define UNITTEST_CONCAT(a,b) UNITTEST_CONCAT2(a, b)

#define UNITTEST_CLASS_NAME(name) \
    UNITTEST_CONCAT(unit_, name)

#define UNITTEST_INSTANCE_NAME(name) \
    UNITTEST_CONCAT(UNITTEST_CLASS_NAME(name), _instance)

#define UNITTESTIMPL(name, init_enabled, fn_attr) \
class UNITTEST_CLASS_NAME(name) \
    : public unittest::unit { \
public: \
    UNITTEST_CLASS_NAME(name)() \
        : unit(#name, __FILE__, __LINE__, (init_enabled)) {} \
    void invoke() override final; \
}; UNITTEST_CLASS_NAME(name) UNITTEST_INSTANCE_NAME(name); \
fn_attr void UNITTEST_CLASS_NAME(name) ::invoke()

template<typename T, void (unit::*member)(T value)>
struct unittest_set_member {
    unittest_set_member(unit* unit_ptr, T enabled)
    {
        (unit_ptr->*member)(enabled);
    }
};

using unittest_set_enable =
unittest_set_member<bool, &unit::enabled>;

using unittest_set_float =
unittest_set_member<bool, &unit::float_thread>;

#define UNITTEST(name) UNITTESTIMPL(name, true, )
#define OPTIMIZED_UNITTEST(name) UNITTESTIMPL(name, true, _always_optimize)
#define DISABLED_UNITTEST(name) UNITTESTIMPL(name, false,)

#define UNITTEST_SET_ENABLE(name, enabled) \
    unittest::unittest_set_enable \
        UNITTEST_CONCAT(set_unit_set_enable_, name)( \
            (& UNITTEST_INSTANCE_NAME(name)), (enabled))

#define UNITTEST_SET_FLOAT(name, enabled) \
    unittest::unittest_set_float \
        UNITTEST_CONCAT(set_unit_set_float_, name)( \
            (& UNITTEST_INSTANCE_NAME(name)), (enabled))


} // namespace unittest
