#pragma once
#include "types.h"

typedef int(*write_debug_str_handler_t)(char const *str, intptr_t len);

int write_debug_str(char const *str, intptr_t len);

void write_debug_str_set_handler(write_debug_str_handler_t handler);
