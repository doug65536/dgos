#pragma once
#include <stdarg.h>
#include "types.h"
//#include "bioscall.h"
#include "screen_abstract.h"

#include "debug.h"

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

void write_char_dummy(tchar */*buf*/, tchar **/*pptr*/, tchar /*c*/, void *);
void write_debug_dummy(tchar const */*s*/, size_t /*sz*/, void *);

void formatter(tchar const *format, va_list ap,
        void (*write_char)(tchar *buf, tchar **pptr, tchar c, void *) =
            write_char_dummy,
        void *write_char_arg = nullptr,
        void (*write_debug)(tchar const *s, size_t sz, void *) =
            write_debug_dummy,
        void *write_debug_arg = nullptr);

//void dump_regs(bios_regs_t& regs, bool show_flags = false);

//#define PRINT(...) print_line(TSTR __VA_ARGS__)
#define PRINT(...) (printdbg_dummy(__VA_ARGS__),printdbg(TSTR __VA_ARGS__))
#define VPRINT(...) (vprintdbg_dummy(__VA_ARGS__),printdbg(TSTR __VA_ARGS__))

extern bool screen_enabled;

__END_DECLS
