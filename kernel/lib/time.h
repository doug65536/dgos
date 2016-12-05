#pragma once
#include "types.h"

extern uint64_t (*time_ms)(void);
extern uint32_t (*usleep)(uint16_t microsec);
void sleep(int ms);
