#pragma once
#include "types.h"

typedef struct table_register_64_t {
    uint16_t limit;
    uint16_t base_lo;
    uint16_t base_hi;
    uint16_t base_hi1;
    uint16_t base_hi2;
} table_register_64_t;

#define GDT_SEG_KERNEL_CS  0x08
#define GDT_SEG_KERNEL_DS  0x10
#define GDT_SEG_USER_CS  0x18
#define GDT_SEG_USER_DS  0x20

int init_gdt(void);
