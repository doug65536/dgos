#pragma once

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdlib.h>
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
};

surface_t *surface_create(int32_t width, int32_t height);
void surface_free(surface_t *pp);
void surface_autofree(surface_t **pp);

static inline uint32_t const *surface_pixels(surface_t const *s)
{
    return s->pixels;
}

#define autofree_png __attribute__((__cleanup__(png_free)))

/// Scrolling text ring
/// Represented as a circular ring of rows
/// Scrolling the display up is a zero copy operation, just clear the new
/// line and advance the index to the oldest row
/// When the bottom of the ring and the top of the ring are in the viewport
/// simultaneously, do a pair of blts
/// Named VGA because it is specialized to draw VGA fonts fast
class vga_console_ring_t : public surface_t {
public:
    vga_console_ring_t(int32_t width, int32_t height)
        : surface_t(width, height, (uint32_t*)(this + 1))
    {
    }

    void reset();

    void new_line();

private:
    // Font dimensions in pixels
    uint32_t font_w = 9, font_h = 16;

    // Size of ring in glyphs
    uint32_t char_w, char_h;

    // The row index of the top row when scrolled all the way to the top
    uint32_t oldest_row;

    // The distance from the oldest row to the row at the top of the screen
    uint32_t scroll_top;

    // The y coordinates and heights of the region visible on
    // top region (band0) and bottom region (band1) of the screen
    uint32_t band0_y, band0_h;
    uint32_t band1_y, band1_h;

    // Number of rows actually emitted to the output
    uint32_t row_count;
};
