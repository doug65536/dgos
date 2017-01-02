#pragma once
#include "types.h"

void time_ms_set_handler(uint64_t (*vec)(void));
void usleep_set_handler(uint32_t (*vec)(uint16_t microsec));

uint64_t time_ms(void);
uint32_t usleep(uint16_t microsec);
void sleep(int ms);
