#pragma once
#include "types.h"

typedef struct time_of_day_t {
    uint8_t centisec;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} time_of_day_t;

void cmos_init(void);
void cmos_prepare_ap(void);
time_of_day_t cmos_gettimeofday(void);
