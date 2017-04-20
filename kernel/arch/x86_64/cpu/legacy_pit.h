#pragma once
#include "types.h"

void pit8253_init();
void pit8254_enable();
uint32_t pit8253_nsleep(uint16_t us);
