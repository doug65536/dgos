#include "dev_text.h"

#include "vga.h"
#include "bios_data.h"
#include "mm.h"
#include "string.h"
#include "cpu/ioport.h"
#include "time.h"
#include "callout.h"

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

    int mouse_x;
    int mouse_y;
    int mouse_on;

    uint16_t shadow[80*25];
};

#define MAX_VGA_DISPLAYS 2

#define VGA_CRT_CTRL    (self->io_base)

#define VGA_SET_CURSOR_LO(pos) (0x0F | (((pos) << 8) & 0xFF00))
#define VGA_SET_CURSOR_HI(pos) (0x0E | ((pos) & 0xFF00))

static text_display_t displays[MAX_VGA_DISPLAYS];

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

static int mouse_toggle(text_display_t *self,
                 int show)
{
    if (!self->mouse_on != !show) {
        // Invert color at mouse
        size_t place = self->mouse_y *
                self->width +
                self->mouse_x;
        uint8_t attr =
                (self->shadow[place] >> 8) & 0xFF;
        uint8_t set_attr = show
                ? ((attr >> 4) | (attr << 4)) & 0x7F
                : attr;

        // Force visible if showing in cell with same color
        // for foreground and background
        if (show && ((set_attr >> 4) == (set_attr & 0x0F)))
            set_attr ^= 7 << 4;

        self->video_mem[place] =
                      (self->shadow[place] & 0xFF) |
                      (set_attr << 8);
        self->mouse_on = !!show;
        return !show;
    }
    return !!show;
}

static int mouse_hide_if_at(text_display_t *self,
                            int x, int y)
{
    if (self->mouse_on &&
            self->mouse_x == x &&
            self->mouse_y == y) {
        return mouse_toggle(self, 0);
    }
    return self->mouse_on;
}

static int mouse_hide_if_within(text_display_t *self,
                                int sx, int sy,
                                int ex, int ey)
{
    if (self->mouse_on &&
            self->mouse_x >= sx &&
            self->mouse_x < ex &&
            self->mouse_y >= sy &&
            self->mouse_y < ey) {
        return mouse_toggle(self, 0);
    }
    return self->mouse_on;
}

static void move_cursor_to(text_display_t *self, int x, int y)
{
    uint16_t position = y * self->width + x;
    outw(VGA_CRT_CTRL, VGA_SET_CURSOR_LO(position));
    outw(VGA_CRT_CTRL, VGA_SET_CURSOR_HI(position));
}

static void move_cursor_if_on(text_display_t *self)
{
    if (self->cursor_on)
        move_cursor_to(self,
                       self->cursor_x,
                       self->cursor_y);
}

static void write_char_at(
        text_display_t *self,
        int x, int y,
        int character, int attrib)
{
    int mouse_was_shown = mouse_hide_if_at(self, x, y);
    size_t place = y * self->width + x;
    uint16_t pair = (character & 0xFF) | ((attrib & 0xFF) << 8);
    self->shadow[place] = pair;
    self->video_mem[place] = pair;
    mouse_toggle(self, mouse_was_shown);
}

static void clear_screen(text_display_t *self)
{
    int mouse_was_shown = mouse_toggle(self, 0);
    uint16_t *p = self->shadow;
    for (int y = 0; y < self->height; ++y)
        for (int x = 0; x < self->width; ++x)
            *p++ = ' ' | (self->attrib << 8);
    memcpy(self->video_mem, self->shadow, sizeof(self->shadow));
    mouse_toggle(self, mouse_was_shown);
}

static void fill_region(text_display_t *self,
                        int sx, int sy,
                        int ex, int ey,
                        int character)
{
    int mouse_was_shown = mouse_hide_if_within(
                self, sx, sy, ex, ey);
    int row_count = ey - sy;
    int row_ofs;
    uint16_t *dst = self->shadow + (sy * self->width);

    character &= 0xFF;
    while (row_count--) {
        for (row_ofs = sx; row_ofs < ex; ++row_ofs)
            dst[row_ofs] = character | (self->attrib << 8);
        dst += self->width;
    }
    memcpy(self->video_mem + self->width * sy,
           self->shadow + self->width * sy,
           sizeof(*self->video_mem) * (ey - sy + 1));
    mouse_toggle(self, mouse_was_shown);
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
        move_cursor_if_on(self);
        return;
    }

    if (y <= 0) {
        // Up
        src = self->shadow - (y * self->width);
        dst = self->shadow;
        row_count = self->height + y;
        // Loop from top to bottom
        row_step = self->width;
        // Clear the bottom
        clear_end_row = self->height;
        clear_start_row = clear_end_row + y;
    } else {
        // Down
        dst = self->shadow + ((self->height - 1) * self->width);
        src = self->shadow + ((self->height - y - 1) * self->width);
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
        memmove(dst, src, row_size * sizeof(*dst));
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

    int mouse_was_shown = mouse_toggle(self, 0);
    memcpy(self->video_mem, self->shadow,
           self->width * self->height * sizeof(*self->shadow));
    mouse_toggle(self, mouse_was_shown);
}

// Advance the cursor, wrapping and scrolling as necessary.
// Does not update hardware cursor.
static void advance_cursor(text_display_t *self, int distance)
{
    self->cursor_x += distance;
    if (self->cursor_x >= self->width) {
        self->cursor_x = 0;
        if (++self->cursor_y >= self->height) {
            self->cursor_y = self->height - 1;
            scroll_screen(self, 0, -1);
        }
    }
}

// Handle linefeed and tab, advance cursor,
// put character on the screen, but does not
// update the hardware cursor
static void print_character(text_display_t *self, int ch)
{
    int advance;
    switch (ch) {
    case '\n':
        advance_cursor(self, self->width - self->cursor_x);
        break;

    case '\t':
        advance = ((self->cursor_x + 8) & -8) -
                self->cursor_x;
        advance_cursor(self, advance);
        break;

    case '\b':
        if (self->cursor_x > 0)
            advance_cursor(self, -1);
        break;

    default:
        write_char_at(self,
                      self->cursor_x,
                      self->cursor_y,
                      ch, self->attrib);
        advance_cursor(self, 1);
        break;
    }
}

// === Public API ===

static int vga_detect(text_display_base_t **result)
{
    text_display_t *self = displays;
    self->vtbl = &vga_device_vtbl;
    self->io_base = *BIOS_DATA_AREA(uint16_t, 0x463);
    self->video_mem = (void*)0xB8000;
    self->cursor_on = 1;
    self->cursor_x = 0;
    self->cursor_y = 0;
    self->width = 80;
    self->height = 25;
    self->attrib = 0x07;
    self->mouse_on = 0;
    self->mouse_x = 40;
    self->mouse_y = 12;
    mouse_toggle(self, 1);

    *result = (text_display_base_t*)self;

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

static void vga_remap_callback(void *arg)
{
    (void)arg;
    for (size_t i = 0; i < countof(displays); ++i)
        displays[i].video_mem = mmap(
                    (void*)0xB8000, 0x8000,
                    PROT_READ | PROT_WRITE,
                    MAP_PHYSICAL, -1, 0);
}

REGISTER_CALLOUT(vga_remap_callback, 0, 'V', "000");

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
        move_cursor_if_on(self);
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

static int vga_cursor_toggle(text_display_base_t *dev,
                              int show)
{
    TEXT_DEV_PTR(dev);
    int was_shown = self->cursor_on;

    if (!show != !self->cursor_on) {
        self->cursor_on = show;
        if (!show)
            move_cursor_to(self, 0xFF, 0xFF);
        else
            move_cursor_to(self, self->cursor_x, self->cursor_y);
    }

    return was_shown;
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
    print_character(self, character);
    move_cursor_if_on(self);
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

    print_character(self, character);
}

static int vga_print(text_display_base_t *dev,
                     char const *s)
{
    TEXT_DEV_PTR(dev);

    int written = 0;
    while (*s) {
        ++written;
        print_character(self, *s++);
    }

    move_cursor_if_on(self);

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
                       int x, int y,
                       char const *s, int attrib)
{
    TEXT_DEV_PTR(dev);

    while (*s) {
        if (x >= self->width) {
            x = 0;
            if (++y >= self->height)
                y = 0;
        }

        write_char_at(self, x++, y, *s++, attrib);
    }
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


// Returns 0 if mouse is not supported
static int vga_mouse_supported(text_display_base_t *dev)
{
    TEXT_DEV_PTR_UNUSED(dev);
    return 1;
}

// Show/hide mouse
static int vga_mouse_is_shown(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return self->mouse_on;
}

// Get mouse position
static int vga_mouse_get_x(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return self->mouse_x;
}

static int vga_mouse_get_y(text_display_base_t *dev)
{
    TEXT_DEV_PTR(dev);
    return self->mouse_y;
}

// Show/hide mouse
static int vga_mouse_toggle(text_display_base_t *dev,
                    int show)
{
    TEXT_DEV_PTR(dev);
    return mouse_toggle(self, show);
}

// Move mouse to position
static void vga_mouse_goto_xy(text_display_base_t *dev,
                     int x, int y)
{
    TEXT_DEV_PTR(dev);
    int was_shown = mouse_toggle(self, 0);
    cap_position(self, &x, &y);
    self->mouse_x = x;
    self->mouse_y = y;
    mouse_toggle(self, was_shown);
}

static void vga_mouse_add_xy(text_display_base_t *dev,
                     int x, int y)
{
    TEXT_DEV_PTR(dev);
    int was_shown = mouse_toggle(self, 0);
    x += self->mouse_x;
    y += self->mouse_y;
    cap_position(self, &x, &y);
    self->mouse_x = x;
    self->mouse_y = y;
    mouse_toggle(self, was_shown);
}

REGISTER_text_display_DEVICE(vga)
