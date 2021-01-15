#pragma once
#include "types.h"

int timer_create(void (*handler)(void *), void *arg,
                 int64_t when, bool periodic);

bool timer_destroy(int id);

class timer_t {
public:
    using id_t = int;

    timer_t() noexcept = default;

    static constexpr id_t const none = 0;

    id_t timer = none;

    timer_t(timer_t const &) = delete;
    timer_t &operator=(timer_t const&) = delete;

    timer_t(timer_t &&rhs) noexcept
    {
        timer = rhs.release();
    }

    ~timer_t() noexcept
    {
        reset();
    }

    timer_t &operator=(timer_t &&rhs) noexcept
    {
        if (rhs.timer != timer) {
            reset();
            timer = rhs.release();
        }
        return *this;
    }

    id_t release() noexcept
    {
        id_t result = timer;
        timer = none;
        return result;
    }

    void reset(int new_value = none) noexcept
    {
        if (timer != none && timer != new_value)
            timer_destroy(timer);
        timer = new_value;
    }

    operator id_t() const noexcept
    {
        return timer;
    }
};
