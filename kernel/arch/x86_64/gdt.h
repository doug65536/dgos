#pragma once
#include "types.h"

typedef struct table_register_64_t {
    uint16_t limit;
    uint16_t base_lo;
    uint16_t base_hi;
    uint16_t base_hi1;
    uint16_t base_hi2;
} table_register_64_t;

int init_gdt(void);
