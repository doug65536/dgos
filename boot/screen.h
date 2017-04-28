#pragma once

#include "types.h"

#ifdef __GNUC__
#define ATTRIBUTE_FORMAT(m,n) __attribute__((format(printf, m, n)))
#else
#define ATTRIBUTE_FORMAT(m,n)
#endif

void scroll_screen();
void copy_to_screen(uint16_t offset, char const *message, uint8_t attr);

extern "C" void print_line(char const* format, ...) ATTRIBUTE_FORMAT(1, 2);
void print_xy(uint16_t x, uint16_t y, uint16_t ch, uint16_t attr, uint16_t count);
extern char const hexlookup[];

void print_lba(uint32_t lba);
