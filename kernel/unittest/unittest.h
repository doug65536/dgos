#pragma once
#include "types.h"
#include "string.h"

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

class unit {
public:
    unit(char const *name, char const *test_file, int test_line);

    void fail(const char *file, int line);

    void set_ctx(unit_ctx *ctx);

    virtual void invoke() = 0;
    virtual bool enabled() const { return true; }

    void eq(char const *expect, char *value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void eq(T&& expect, U&& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void ne(T&& expect, U&& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void lt(T&& expect, U&& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void gt(T&& expect, U&& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void le(T&& expect, U&& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    template<typename T, typename U>
    void ge(T&& expect, U&& value,
            char const *file = __builtin_FILE(),
            int line = __builtin_LINE());

    char const *get_name() const;
    char const *get_file() const;
    int get_line() const;

    static void run_all(unit_ctx *ctx);

protected:
    unit_ctx *ctx = nullptr;
    unit *next = nullptr;

    static unit *list_st;
    static unit *list_en;

private:
    char const * const name = nullptr;
    char const * const test_file = nullptr;
    int const test_line = 0;
};

template<typename T, typename U>
void unit::eq(T&& expect, U&& value,
        char const *file, int line)
{
    if (!(expect == value))
        fail(file, line);
}

template<typename T, typename U>
void unit::ne(T&& expect, U&& value,
              char const *file, int line)
{
    if (!(expect != value))
        fail(file, line);
}

template<typename T, typename U>
void unit::lt(T&& expect, U&& value,
              char const *file, int line)
{
    if (!(expect < value))
        fail(file, line);
}

template<typename T, typename U>
void unit::gt(T&& expect, U&& value,
              char const *file, int line)
{
    if (!(expect > value))
        fail(file, line);
}

template<typename T, typename U>
void unit::le(T&& expect, U&& value,
              char const *file, int line)
{
    if (!(expect <= value))
        fail(file, line);
}

template<typename T, typename U>
void unit::ge(T&& expect, U&& value,
              char const *file, int line)
{
    if (!(expect >= value))
        fail(file, line);
}

extern template void unit::eq(int&&,int&&,const char *file, int line);
extern template void unit::eq(uint32_t&&,uint32_t&&,const char *file, int line);
extern template void unit::eq(bool&&,bool&&,const char *file, int line);
extern template void unit::eq(size_t&&,size_t&&,const char *file, int line);

#define UNITTEST_CONCAT2(a,b) a##b
#define UNITTEST_CONCAT(a,b) UNITTEST_CONCAT2(a, b)

#define UNITTESTIMPL(name, symbol, is_enabled) \
class symbol \
    : public unittest::unit { \
public: \
    symbol() : unit(#name, __FILE__, __LINE__) {} \
    void invoke() override final; \
    bool enabled() const override final { return (is_enabled); } \
}; symbol UNITTEST_CONCAT(symbol, instance); void symbol ::invoke()

#define UNITTEST(name) \
    UNITTESTIMPL(name, UNITTEST_CONCAT(unit_, name), true)
#define DISABLED_UNITTEST(name) \
    UNITTESTIMPL(name, UNITTEST_CONCAT(unit_, name), false)

} // namespace unittest

