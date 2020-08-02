#include <surface.h>
#include <stdlib.h>
#include <string.h>
#include <sys/likely.h>
#include <new>

#define CHARHEIGHT 16

struct bitmap_glyph_t {
    uint8_t bits[CHARHEIGHT];
};

extern bitmap_glyph_t const _binary_u_vga16_raw_start[];
extern bitmap_glyph_t const _binary_u_vga16_raw_end[];

void surface_free(surface_t *pp)
{
    free(pp);
}

surface_t *surface_create(int32_t width, int32_t height)
{
    size_t sz = sizeof(surface_t) + width * height * sizeof(uint32_t);
    void *mem = malloc(sz);

    if (unlikely(!mem))
        return nullptr;

    surface_t *s = new (width, height) surface_t{ width, height };

    return s;
}

void vga_console_ring_t::reset()
{
    char_w = width / 9;
    char_h = height / 16;

    row_count = 0;

    memset(pixels, 0, sizeof(uint32_t) * width * height);
}

void vga_console_ring_t::new_line()
{

}
