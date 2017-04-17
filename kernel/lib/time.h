#pragma once
#include "types.h"

struct time_of_day_t {
    uint8_t centisec;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
};

typedef time_of_day_t (*time_ofday_handler_t)();

void time_ms_set_handler(uint64_t (*vec)(void));
void usleep_set_handler(uint32_t (*vec)(uint16_t microsec));

uint64_t time_ms(void);
uint32_t usleep(uint16_t microsec);
void sleep(int ms);

uint64_t nano_time(void);

void time_ofday_set_handler(time_ofday_handler_t handler);
time_of_day_t time_ofday(void);
