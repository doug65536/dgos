#pragma once
#include "types.h"
#include "string.h"

namespace unittest {

class unit;

class unit_ctx {
public:
    void fail(unit *test);

    size_t failure_count() const
    {
        return failures;
    }

private:
    size_t failures = 0;
};

class unit {
public:
    unit(char const *name);

    void fail();

    void set_ctx(unit_ctx *ctx);

    virtual void invoke() = 0;

    void eq(char const *expect, char *value)
    {
        if (strcmp(expect, value))
            fail();
    }

    template<typename T, typename U>
    void eq(T&& expect, U&& value)
    {
        if (!(expect == value))
            fail();
    }

    template<typename T, typename U>
    void ne(T&& expect, U&& value)
    {
        if (!(expect != value))
            fail();
    }

    template<typename T, typename U>
    void lt(T&& expect, U&& value)
    {
        if (!(expect < value))
            fail();
    }

    template<typename T, typename U>
    void gt(T&& expect, U&& value)
    {
        if (!(expect > value))
            fail();
    }

    template<typename T, typename U>
    void le(T&& expect, U&& value)
    {
        if (!(expect <= value))
            fail();
    }

    template<typename T, typename U>
    void ge(T&& expect, U&& value)
    {
        if (!(expect >= value))
            fail();
    }

    char const *get_name() const;

    static void run_all(unit_ctx *ctx);

protected:
    unit_ctx *ctx = nullptr;
    unit *next = nullptr;

    static unit *list_st;
    static unit *list_en;

private:
    char const *name = nullptr;
};

#define UNITTEST_CONCAT2(a,b) a##b
#define UNITTEST_CONCAT(a,b) UNITTEST_CONCAT2(a, b)

#define UNITTESTIMPL(name, symbol) \
class symbol \
    : public unittest::unit { \
public: \
    symbol() : unit(#name) {} \
    void invoke() override final; \
}; symbol UNITTEST_CONCAT(symbol, instance); void symbol ::invoke()

#define UNITTEST(name) UNITTESTIMPL(name, UNITTEST_CONCAT(unit_, name))

} // namespace unittest

