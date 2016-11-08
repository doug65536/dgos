#include "dev_text.h"

#include "vga.h"
#include "bios_data.h"
#include "mm.h"
#include "string.h"
#include "ioport.h"

DECLARE_text_display_DEVICE(vga);

struct text_display_t {
    text_display_vtbl_t *vtbl;

    uint16_t *video_mem;

    ioport_t io_base;

    int cursor_x;
    int cursor_y;
    int cursor_on;

    int width;
    int height;

    int attrib;
};

#define MAX_VGA_DISPLAYS 16

#define VGA_CRT_CTRL    (self->io_base + 0x4)

#define VGA_SET_CURSOR_LO(pos) (0x0F | ((pos) & 0xFF))
#define VGA_SET_CURSOR_HI(pos) (0x0E | ((pos >> 8) & 0xFF))

text_display_t displays[MAX_VGA_DISPLAYS];

// === Internal API ===

static void cap_position(text_display_t *self, int *px, int *py)
{
    if (*px < 0)
        *px = 0;
    else if (*px >= self->width)
        *px = self->width - 1;

    if (*py < 0)
        *py = 0;
    else if (*py >= self->height)
        *py = self->height - 1;
}

static void move_cursor_to(text_display_t *self, int x, int y)
{
    uint16_t position = y * self->width + x;
    outw(VGA_CRT_CTRL, VGA_SET_CURSOR_LO(position));
    outw(VGA_CRT_CTRL, VGA_SET_CURSOR_HI(position));
}

static void write_char_at(text_display_t *self,
                   int x, int y, int character)
{
    self->video_mem[y * self->width + x] =
            (character & 0xFF) | (self->attrib << 8);
}

static void clear_screen(text_display_t *self)
{
    uint16_t *p = self->video_mem;
    for (int y = 0; y < self->height; ++y)
        for (int x = 0; x < self->width; ++x)
            *p++ = ' ' | (self->attrib << 8);
}

static void fill_region(text_display_t *self,
                        int sx, int sy,
                        int ex, int ey,
                        int character)
{
    int row_count = ey - sy;
    int row_ofs;
    uint16_t *dst = self->video_mem + (sy * self->width);

    character &= 0xFF;
    while (row_count) {
        for (row_ofs = sx; row_ofs < ex; ++row_ofs)
            dst[row_ofs] = character | (self->attrib << 8);
        dst += self->width;
    }
}

// negative x move the screen content left
// negative y move the screen content up
static void scroll_screen(text_display_t *self, int x, int y)
{
    int row_size = self->width;
    int row_count;
    int row_step;
    int clear_start_row;
    int clear_end_row;
    int clear_start_col;
    int clear_end_col;
    uint16_t *src;
    uint16_t *dst;

    // Extreme move distance clears screen
    if ((x < 0 && -x >= self->width) ||
            (x > 0 && x >= self->width) ||
            (y < 0 && -y >= self->height) ||
            (y > 0 && y >= self->height)) {
        clear_screen(self);
        // Cursor tries to move with content
        self->cursor_x += x;
        self->cursor_y += y;
        cap_position(self, &self->cursor_x, &self->cursor_y);
        if (self->cursor_on)
            move_cursor_to(self, self->cursor_x, self->cursor_y);
        return;
    }

    if (y <= 0) {
        // Up
        src = self->video_mem - (y * self->width);
        dst = self->video_mem;
        row_count = self->height + y;
        // Loop from top to bottom
        row_step = self->width;
        // Clear the bottom
        clear_end_row = self->height;
        clear_start_row = clear_end_row + y;
    } else {
        // Down
        dst = self->video_mem + ((self->height - 1) * self->width);
        src = self->video_mem + ((self->height - y - 1) * self->width);
        row_count = self->height - y;
        // Loop from bottom to top
        row_step = -self->width;
        // Clear the top
        clear_start_row = 0;
        clear_end_row = y;
    }

    if (x <= 0) {
        // Left
        row_size += x;
        src -= x;
        // Clear right side
        clear_start_col = self->width + x;
        clear_end_col = self->width;
    } else {
        // Right
        row_size -= x;
        src += x;
        // Clear left side
        clear_start_col = 0;
        clear_end_col = x;
    }

    while (row_count--) {
        memmove(dst, src, row_size);
        dst += row_step;
        src += row_step;
    }

    // Clear top/bottom
    if (clear_start_row != clear_end_row) {
        fill_region(self,
                    0,
                    clear_start_row,
                    self->width,
                    clear_end_row,
                    ' ');
    }

    // Clear left/right
    if (clear_start_col != clear_end_col) {
        fill_region(self,
                    clear_start_col,
                    clear_start_row ? clear_start_row : clear_end_row,
                    clear_end_col,
                    clear_start_row ? self->height : clear_start_row,
                    ' ');
    }
}

// Move the cursor right one character, wrapping and
// scrolling as necessary.
// Does not update hardware cursor.
static void advance_cursor(text_display_t *self)
{
    if (++self->cursor_x >= self->width) {
        self->cursor_x = 0;
        if (++self->cursor_y >= self->height) {
            self->cursor_y = self->height - 1;
            scroll_screen(self, 0, -1);
        }
    }
}

// === Public API ===

static int vga_detect(text_display_base_t **result)
{
    displays[0].io_base = READ_BIOS_DATA_AREA(uint16_t, 0x463) - 4;
    displays[0].cursor_on = 1;
    displays[0].cursor_x = 0;
    displays[0].cursor_y = 0;
    displays[0].width = 80;
    displays[0].height = 25;
    displays[0].attrib = 0x07;

    *result = (text_display_base_t*)displays;

    return 1;
}


// Startup/shutdown

static void vga_init(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);

    self->video_mem = (uint16_t*)0xB8000;
    //video_mem = mmap((void*)0xB8000, 0x10000,
    //                 PROT_READ | PROT_WRITE,
    //                 MAP_FIXED | MAP_LOCKED |
    //                 MAP_NOCACHE | MAP_PHYSICAL,
    //                 -1, 0);
}

static void vga_cleanup(text_display_base_t *dev)
{
    TEXT_DEV_PTR_UNUSED(dev);
}

// Set/get dimensions

static int vga_set_dimensions(text_display_base_t *dev,
                      int width, int height)
{
    TEXT_DEV_PTR(dev);
    // Succeed if they didn't change it
    return width == self->width && height == self->height;
}

static void vga_get_dimensions(text_display_base_t *dev,
                       int *width, int *height)
{
    TEXT_DEV_PTR(dev);
    *width = self->width;
    *height = self->height;
}


// Set/get cursor position

static void vga_goto_xy(text_display_base_t *dev,
                        int x, int y)
{
    TEXT_DEV_PTR(dev);
    cap_position(self, &x, &y);
    if (self->cursor_x != x || self->cursor_y != y) {
        self->cursor_x = x;
        self->cursor_y = y;
        move_cursor_to(self, x, y);
    }
}

static int vga_get_x(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return self->cursor_x;
}

static int vga_get_y(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return self->cursor_y;
}

// Set/get foreground color

static void vga_fg_set(text_display_base_t *dev,
                       int color)
{
    TEXT_DEV_PTR(dev);
    self->attrib = (self->attrib & 0xF0) |
            (color & 0x0F);
}

static int vga_fg_get(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return self->attrib & 0x0F;
}

// Set/get background color

static void vga_bg_set(text_display_base_t *dev,
                       int color)
{
    TEXT_DEV_PTR(dev);
    self->attrib = (self->attrib & 0x0F) |
            ((color & 0x0F) << 4);
}

static int vga_bg_get(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return (self->attrib >> 4) & 0x0F;
}

// Show/hide cursor

static void vga_cursor_toggle(text_display_base_t *dev,
                              int show)
{
    TEXT_DEV_PTR(dev);
    if (!show != !self->cursor_on) {
        show = self->cursor_on;
        if (!show)
            move_cursor_to(self, 0xFF, 0xFF);
        else
            move_cursor_to(self, self->cursor_x, self->cursor_y);
    }
}

static int vga_cursor_is_shown(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return !!self->cursor_on;
}

// Print character, updating cursor position
// possibly scrolling the screen content up

static void vga_putc(text_display_base_t *dev,
                    int character)
{
    TEXT_DEV_PTR(dev);
    write_char_at(self,
                  self->cursor_x,
                  self->cursor_y,
                  character);
    advance_cursor(self);
}

static void vga_putc_xy(text_display_base_t *dev,
                       int x, int y,
                       int character)
{
    TEXT_DEV_PTR(dev);
    cap_position(self, &x, &y);
    if (self->cursor_on &&
            (self->cursor_x != x || self->cursor_y != y))
        move_cursor_to(self, x, y);

    self->cursor_x = x;
    self->cursor_y = y;

    write_char_at(self,
                  self->cursor_x,
                  self->cursor_y,
                  character);

    advance_cursor(self);
}

// Print string, updating cursor position
// possibly scrolling the screen content up

static int vga_print(text_display_base_t *dev,
                     char const *s)
{
    TEXT_DEV_PTR_UNUSED(dev);
    int written = 0;
    while (*s) {
        vga_putc(dev, *s++);
        ++written;
    }
    return written;
}

static int vga_print_xy(text_display_base_t *dev,
                        int x, int y, char const *s)
{
    TEXT_DEV_PTR_UNUSED(dev);
    vga_goto_xy(dev, x, y);
    return vga_print(dev, s);
}

// Draw text, no effect on cursor

static int vga_draw(text_display_base_t *dev,
                    char const *s)
{
    TEXT_DEV_PTR_UNUSED(dev);
    // FIXME
    (void)s;
    return 0;
}

static int vga_draw_xy(text_display_base_t *dev,
                       int x, int y, char const *s)
{
    TEXT_DEV_PTR_UNUSED(dev);
    // FIXME
    (void)x; (void)y; (void)s;
    return 0;
}

// Fill an area with a character, and optionally a color

static void vga_fill(text_display_base_t *dev,
                     int sx, int sy,
                     int ex, int ey,
                     int character)
{
    TEXT_DEV_PTR(dev);
    fill_region(self, sx, sy, ex, ey, character);
}


// Fill screen with spaces in the current colors
static void vga_clear(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    clear_screen(self);
}

// Scroll an area horizontally and/or vertically
// Optionally clearing exposed area

static void vga_scroll(text_display_base_t *dev,
                       int sx, int sy,
                       int ex, int ey,
                       int xd, int yd,
                       int clear)
{
    TEXT_DEV_PTR_UNUSED(dev);
    // FIXME
    (void)sx; (void)sy; (void)ex; (void)ey; (void)xd; (void)yd;
    (void)clear;
}


REGISTER_text_display_DEVICE(vga)
