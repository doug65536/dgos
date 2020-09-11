#pragma once

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/likely.h>

struct surface_t {
    int32_t width;
    int32_t height;
    uint32_t *pixels;

    surface_t(int32_t width, int32_t height, uint32_t *pixels)
        : width(width)
        , height(height)
        , pixels(pixels)
    {
    }

    surface_t(int32_t width, int32_t height)
        : surface_t(width, height, (uint32_t*)(this + 1))
    {
    }

    void *operator new(size_t type_size, int32_t width, int32_t height)
    {
        size_t sz = type_size + width * height * sizeof(uint32_t);

        void *mem = malloc(sz);

        if (unlikely(!mem))
            return nullptr;

        return mem;
    }

    void operator delete(void *p)
    {
        free(p);
    }

    void fill(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey,
              uint32_t color);
};

surface_t *surface_create(int32_t width, int32_t height);
void surface_free(surface_t *pp);
void surface_autofree(surface_t **pp);

static inline uint32_t const *surface_pixels(surface_t const *s)
{
    return s->pixels;
}

#define autofree_png __attribute__((__cleanup__(png_free)))

#define CHARHEIGHT 16

struct bitmap_glyph_t {
    uint8_t bits[CHARHEIGHT];
};

/// Scrolling text ring
/// Represented as a circular ring of rows.
/// Scrolling the display up is a zero copy operation, just clear the new
/// line and advance the index to the oldest row. The start is moved down
/// a row and the newly exposed old row is cleared.
/// When the bottom of the ring and the top of the ring are in the viewport
/// simultaneously, do a pair of blts. The top portion of the screen gets the
/// portion from the first displayed row until the bottom of the surface,
/// the bottom portion of the screen gets the remainder starting from the
/// top of the surface.
/// Renders a blinking cursor at any character position.
/// Named VGA because it is specialized to draw VGA bitmap fonts fast.
class vga_console_ring_t : public surface_t {
public:
    vga_console_ring_t(int32_t width, int32_t height);

    void reset();

    void new_line(uint32_t color);

    // Font dimensions in pixels
    static constexpr uint32_t font_w = 9;
    static constexpr uint32_t font_h = 16;

    inline uint32_t wrap_row(uint32_t row) const
    {
        assert(row < ring_h * 2U);
        return row < ring_h ? row : (row - ring_h);
    }

    inline uint32_t wrap_row_signed(uint32_t row) const
    {
        // "signed" meaning, handles the case where the row wrapped around
        // past zero and became an enormous unsigned number
        // unsigned(-1) would wrap around to the last row
        // row must be within char_h rows of a valid range off either end
        assert(row + ring_h < ring_h * 3U);
        return row >= -ring_h
                ? row + ring_h
                : row >= ring_h
                  ? row - ring_h
                  : row;
    }

    // Clear the specified row, in surface space
    void clear_row(uint32_t row, uint32_t color);

    void write(char const *data, size_t size, uint32_t bg, uint32_t fg);

private:
    size_t glyph_index(size_t codepoint);
    bitmap_glyph_t const *get_glyph(char32_t codepoint);

    void bit8_to_pixels(uint32_t *out, uint8_t bitmap,
                        uint32_t bg, uint32_t fg);

    // The place where writing text will go in surface space
    // This is advanced by writing text and newlines
    uint32_t out_x = 0, out_y = 0;

    // Size of ring in glyphs
    uint32_t ring_w, ring_h;

    // The row index of the top row when scrolled all the way to the top
    uint32_t oldest_row;

    // The distance from the oldest row to the row at the top of the screen
    uint32_t scroll_top;

    // The y coordinates and heights of the region visible on
    // top region (band0) and bottom region (band1) of the screen
    uint32_t band0_y, band0_h;
    uint32_t band1_y, band1_h;

    // Number of rows actually emitted to the output
    // Indicates how much of the surface has actually been initialized
    uint32_t row_count;

    // The standard VGA blinked every 16 frames,
    // 16/60 blinks per second, 266 and 2/3 ms per blink, 3.75 blinks/sec
    uint32_t blink_cycle_ns = 266666666;

    // A flashing cursor is shown at this location
    uint32_t crsr_x, crsr_y;

    // The start and end scanline of the cursor in the character cell
    uint32_t crsr_s, crsr_e;
};

// Record that represents a single box of area on the screen
struct comp_area_t {
    // This screen area...
    int32_t sy;
    int32_t sx;
    int32_t ey;
    int32_t ex;

    // ...contains this area of this surface
    surface_t *surface;
    int32_t y;
    int32_t x;
};
