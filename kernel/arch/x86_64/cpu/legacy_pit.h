#pragma once
#include "types.h"

void pit8254_init();
void pit8254_enable();
uint32_t pit8254_nsleep(uint16_t us);
