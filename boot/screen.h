#pragma once
#include <stdarg.h>
#include "types.h"
#include "bioscall.h"
#include "screen_abstract.h"

__BEGIN_DECLS

#if defined(__GNUC__) && !defined(__efi)
#define ATTRIBUTE_FORMAT(m,n) __attribute__((__format__(__printf__, (m), (n))))
#else
#define ATTRIBUTE_FORMAT(m,n)
#endif

void scroll_screen(uint8_t attr);

void print_at(int col, int row, uint8_t attr,
              size_t length, tchar const *text);

void copy_to_screen(uint16_t offset, char const *message, uint8_t attr);

ATTRIBUTE_FORMAT(4, 0)
void vprint_line_at(int x, int y, int attr, tchar const *format, va_list ap);

ATTRIBUTE_FORMAT(4, 5)
void print_line_at(int x, int y, int attr, tchar const *format, ...);

ATTRIBUTE_FORMAT(2, 0)
void vprint_line(int attr, tchar const *format, va_list ap);

ATTRIBUTE_FORMAT(1, 2)
void print_line(tchar const* format, ...);

void print_xy(int x, int y, tchar ch, uint16_t attr, size_t count);

void print_str_xy(int x, int y, tchar const *s, size_t len,
                  uint16_t attr, size_t min_len);

extern char const hexlookup[];

void print_box(int left, int top, int right, int bottom, int attr, bool clear);

void print_lba(uint32_t lba);

static inline tchar boxchar_solid()
{
    return boxchars[S];
}

//void dump_regs(bios_regs_t& regs, bool show_flags = false);

#define PRINT(...) print_line(TSTR __VA_ARGS__)

__END_DECLS
