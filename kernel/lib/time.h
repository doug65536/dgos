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

bool time_ns_set_handler(uint64_t (*vec)(void), void (*stop)(), bool override);
bool nsleep_set_handler(uint64_t (*vec)(uint64_t microsec), void (*stop)(),
                        bool override);

uint64_t time_ns(void);
uint64_t nsleep(uint64_t nanosec);
void sleep(int ms);

// Returns counter at unknown rate. Use following helpers to translate to
// actual time duration and back.
uint64_t nano_time(void);

_const
uint64_t nano_time_ns(uint64_t a, uint64_t b);

_const
uint64_t nano_time_add(uint64_t after, uint64_t ns);

void time_ofday_set_handler(time_ofday_handler_t handler);
time_of_day_t time_ofday(void);

uint64_t time_unix(time_of_day_t const& time);
uint64_t time_unix_ms(time_of_day_t const& time);

__END_DECLS
