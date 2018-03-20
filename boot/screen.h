#pragma once

#include "types.h"
#include "bioscall.h"

#ifdef __GNUC__
#define ATTRIBUTE_FORMAT(m,n) __attribute__((format(printf, m, n)))
#else
#define ATTRIBUTE_FORMAT(m,n)
#endif

void scroll_screen();
void copy_to_screen(uint16_t offset, char const *message, uint8_t attr);

extern "C" void print_line(char const* format, ...) ATTRIBUTE_FORMAT(1, 2);
void print_xy(int x, int y, uint8_t ch, uint16_t attr, size_t count);
void print_str_xy(int x, int y, char const *s, size_t len,
                  uint16_t attr, size_t min_len);

extern char const hexlookup[];


void print_box(int left, int top, int right, int bottom, int attr, bool clear);

void print_lba(uint32_t lba);

void dump_regs(bios_regs_t& regs, bool show_flags = false);
