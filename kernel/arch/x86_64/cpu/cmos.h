#pragma once
#include "types.h"
#include "time.h"

void cmos_init(void);
void cmos_prepare_ap(void);
time_of_day_t cmos_gettimeofday(void);
