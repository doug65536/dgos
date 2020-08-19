#include <surface.h>
#include <stdlib.h>
#include <string.h>
#include <sys/likely.h>
#include <new>
#include "../../user/include/utf.h"

// Linked in font
extern bitmap_glyph_t const _binary_u_vga16_raw_start[];
extern bitmap_glyph_t const _binary_u_vga16_raw_end[];

size_t vga_console_ring_t::replacement;

static constexpr size_t const ascii_min = 32;
static constexpr size_t const ascii_max = 126;
static uint8_t ascii_lookup[1 + ascii_max - ascii_min];

static size_t glyph_count;
static char16_t *glyph_codepoints;

static constexpr bitmap_glyph_t const * const glyphs =
        _binary_u_vga16_raw_start;

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

vga_console_ring_t::vga_console_ring_t(int32_t width, int32_t height)
    : surface_t(width, height, (uint32_t*)(this + 1))
{
    // Lookup the unicode replacement character glyph
    replacement = glyph_index(0xFFFD);
}

void vga_console_ring_t::reset()
{
    ring_w = width / 9;
    ring_h = height / 16;

    row_count = 0;

    memset(pixels, 0, sizeof(uint32_t) * width * height);
}

void vga_console_ring_t::new_line(uint32_t color)
{
    crsr_x = 0;

    uint32_t exposed_row;

    if (crsr_y + 1 == ring_h) {
        // Advance the ring

        oldest_row = wrap_row(oldest_row + 1);

        exposed_row = wrap_row(oldest_row + ring_h - 1);
    } else {
        exposed_row = row_count++;
    }

    clear_row(exposed_row, color);
}

void vga_console_ring_t::clear_row(uint32_t row, uint32_t color)
{
    assert(row < ring_h);
    uint32_t sx = 0;
    uint32_t sy = row * font_h;
    uint32_t ex = ring_w * font_w;
    uint32_t ey = sy + font_h;

    fill(sx, sy, ex, ey, color);
}

void vga_console_ring_t::bit8_to_pixels(uint32_t *out, uint8_t bitmap,
                                        uint32_t bg, uint32_t fg)
{
    out[0] = bitmap & 0x80 ? fg : bg;
    out[1] = bitmap & 0x40 ? fg : bg;
    out[2] = bitmap & 0x20 ? fg : bg;
    out[3] = bitmap & 0x10 ? fg : bg;
    out[4] = bitmap & 0x08 ? fg : bg;
    out[5] = bitmap & 0x04 ? fg : bg;
    out[6] = bitmap & 0x02 ? fg : bg;
    out[7] = bitmap & 0x01 ? fg : bg;
}

void vga_console_ring_t::write(const char *data, size_t size,
                               uint32_t bg, uint32_t fg)
{
    // 16 32-bit pixels == 1 cache line of output pixels
    size_t constexpr cp_max = 16;
    char32_t codepoints[cp_max];

    size_t row_remain = ring_w - out_x;

    while (size) {
        size_t cp = 0;

        while (size && cp < 16 && cp < row_remain) {
            codepoints[cp++] = utf8_to_ucs4_upd(data);
            --size;
        }

        bitmap_glyph_t const *glyphs[cp_max];

        for (size_t i = 0; i < cp; ++i)
            glyphs[i] = get_glyph(codepoints[i]);

        // Draw glyph into surface

        // 16 scanlines per row
        uint32_t *row = pixels + width * font_h * out_y + font_w * out_x;

        // Doing loop interchange to write whole cache line of output across
        // Doing 0th scanline of each glyph, then 1st of each, then 2nd, etc

        for (size_t y = 0; y < 16; ++y, row += width) {
            // Up to 16 characters across

            uint32_t *out = row;

            for (size_t i = 0; i < cp; ++i, out += font_w)
                bit8_to_pixels(out, glyphs[i]->bits[y], bg, fg);
        }

        out_x += cp;

        // If at end of row
        if (out_x == ring_w) {
            // Move to start of
            out_x = 0;

            // Next row
            ++out_y;

            // If out of range
            if (out_y == ring_h) {
                // Bump back into range
                --out_y;

                // Make a new row
                new_line(0);
            } else if (out_y == row_count) {
                // Make a new row
                new_line(0);
            }
        }
    }
}

size_t vga_console_ring_t::glyph_index(size_t codepoint)
{
    if (likely(replacement &&
               codepoint >= ascii_min &&
               codepoint <= ascii_max))
        return ascii_lookup[codepoint - ascii_min];

    size_t st = 0;
    size_t en = glyph_count;
    size_t md;

    while (st < en) {
        md = ((en - st) >> 1) + st;
        char32_t gcp = glyph_codepoints[md];
        st = gcp <  codepoint ? md + 1 : st;
        en = gcp >= codepoint ? md : en;
    }

    if (unlikely(st >= glyph_count || glyph_codepoints[st] != codepoint))
        return size_t(-1);

    return st;
}

bitmap_glyph_t const *vga_console_ring_t::get_glyph(char32_t codepoint)
{
    size_t i = glyph_index(codepoint);

    return i != size_t(-1)
            ? &glyphs[i]
            : replacement
              ? &glyphs[replacement]
              : &glyphs[ascii_lookup[' ' - ascii_min]];
}

void surface_t::fill(uint32_t sx, uint32_t sy,
                     uint32_t ex, uint32_t ey, uint32_t color)
{
    assert(ex >= sx);
    assert(ey >= sy);

    uint32_t *out = pixels + (sy * width);

    for (uint32_t y = sy; y < ey; ++y, out += width) {
        for (uint32_t x = sx; x < ex; ++x)
            out[x] = color;
    }
}
