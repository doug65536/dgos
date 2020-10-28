#pragma once
#include "types.h"

__BEGIN_DECLS

struct time_of_day_t {
    uint8_t centisec;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;

    inline constexpr int fullYear() const {
        return century * 100 + year;
    }
};

typedef time_of_day_t (*time_ofday_handler_t)();

KERNEL_API bool time_ns_set_handler(uint64_t (*vec)(void),
                                    void (*stop)(), bool override);
KERNEL_API bool nsleep_set_handler(uint64_t (*vec)(uint64_t microsec),
                                   void (*stop)(), bool override);

KERNEL_API uint64_t time_ns(void);
KERNEL_API uint64_t nsleep(uint64_t nanosec);
KERNEL_API void sleep(int ms);

// Returns counter at unknown rate. Use following helpers to translate to
// actual time duration and back.
KERNEL_API uint64_t nano_time(void);

_const
KERNEL_API uint64_t nano_time_ns(uint64_t a, uint64_t b);

_const
KERNEL_API uint64_t nano_time_add(uint64_t after, uint64_t ns);

KERNEL_API void time_ofday_set_handler(time_ofday_handler_t handler);
KERNEL_API time_of_day_t time_ofday(void);

KERNEL_API uint64_t time_unix(time_of_day_t const& time);
KERNEL_API uint64_t time_unix_ms(time_of_day_t const& time);

__END_DECLS
