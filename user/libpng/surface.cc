#include <surface.h>
#include <stdlib.h>
#include <string.h>
#include <sys/likely.h>
#include <new>
#include "../../user/include/utf.h"

// The screen is decomposed into a series of boxes that
// correspond to offscreen surface areas
// Adding a box to the screen completely removes the areas that are
// completely obscured by the new box, and clips off partially overlapping
// postions of existing boxes

///   clip left       span entire     clip right       within
/// +--------------+--------------+--------------+--------------+
/// |              |              |              |              |
/// | +-----+      | +----------+ |      +-----+ |    +----+    |
/// | |a   b|      | |a        b| |      |a   b| |    |a  b|    |
/// | |   + |---+  | |  +    +  | |  +---| +   | | +--|    |--+ |
/// | |c   d|m n|  | |c        d| |  |i j|c   d| | |ij|    |mn| | clip top
/// | +-----+o p|  | +----------+ |  |k l+-----+ | |  |    |  | |
/// |     |q   r|  |    |q  r|    |  |q   r|     | |  |c  d|  | |
/// |     |     |  |    |    |    |  |     |     | |kl+----+op| |
/// |     |s   t|  |    |s  t|    |  |s   t|     | |q        r| |
/// |     +-----+  |    +----+    |  +-----+     | |s        t| |
/// |             3|             2|             3| +----------+4|
/// +--------------+--------------+--------------+--------------+
/// |              |              |              |              |
/// | +-----+      | +----------+ |      +-----+ |    +----+    |
/// | |a   b|      | |a        b| |      |a   b| |    |a  b|    |
/// | |   + |---+  | |  +    +  | |  +---|  +  | | +--|    |--+ |
/// | |     |m n|  | |          | |  |i j|     | | |ij|    |mn| |
/// | |     |   |  | |          | |  |   |     | | |  |    |  | | span entire
/// | |     |o p|  | |          | |  |k l|     | | |  |    |  | |
/// | |   + |---+  | |  +    +  | |  +---|  +  | | |  |    |  | |
/// | |c   d|      | |c        d| |      |c   d| | |kl|    |op| |
/// | +-----+      | +----------+ |      +-----+ | +--|c  d|--+ |
/// |             2|             1|             2|    +----+   3|
/// +--------------+--------------+--------------+--------------+
/// |              |              |              |              |
/// |    +------+  |    +----+    | +-------+    | +----------+ |
/// |    |e    f|  |    |e  f|    | |e     f|    | |e        f| |
/// |    |      |  |    |    |    | |       |    | |          | |
/// |    |g    h|  |    |g  h|    | |g     h|    | |g        h| |
/// | +-----+m n|  | +----------+ | |i   j+----+ | |ij+----+mn| | clip bottom
/// | |a   b|   |  | |a        b| | |k   l|a  b| | |kl|a  b|op| |
/// | |     |o p|  | |  +    +  | | +-----| +  | | +--|    |--+ |
/// | |c + d|---+  | |c        d| |       |c  d| |    |c  d|    |
/// | +-----+      | +----------+ |       +----+ |    +----+    |
/// |             3|             2|             3|             4|
/// +--------------+--------------+--------------+--------------+
/// |    +------+  |   +------+   |    +-----+   | +----------+ |
/// |    |e    f|  |   |e    f|   |    |e   f|   | |e        f| |
/// |    |g    h|  |   |g    h|   |    |g   h|   | |g        h| |
/// | +-----+m n|  | +----------+ |    |i j+---+ | |ij+----+mn| |
/// | |a   b|   |  | |a        b| |    |   |a b| | |  |a  b|  | |
/// | |     |   |  | |          | |    |   |   | | |  |    |  | | within
/// | |c   d|   |  | |c        d| |    |   |c d| | |  |c  d|  | |
/// | +-----+o p|  | +----------+ |    |k l+---+ | |kl+----+op| |
/// |    |q    r|  |   |q    r|   |    |q   r|   | |q        r| |
/// |    |s    t|  |   |s    t|   |    |s   t|   | |s        t| |
/// |    +------+ 4|   +------+  3|    +-----+  4| +----------+5|
/// +--------------+--------------+--------------+--------------+
///                                                     ^
///                                             most general case
/// I is the incoming rectangle abcd
/// E is the existing rectangle
///   (efgh (top), ijkl (left), mnop (right), qrst (bottom))
/// ijmn y are the max of the two top edges
/// klop y are the min of the two bottom edges
/// egikqs are always horizontally aligned with left edge of E
/// fhnprt are always horizontally aligned with right edge of E
/// ef are always vertically aligned with the top of E
/// st are always vertically aligned with the bottom of E
/// e, g, q, s are always aligned

static size_t font_replacement;

comp_area_t *comp_areas;
size_t comp_areas_capacity;
size_t comp_areas_size;

static bool grow_areas()
{
    size_t new_capacity;

    if (comp_areas_capacity)
        new_capacity = comp_areas_capacity * 2;
    else
        new_capacity = 16;

    comp_area_t *new_comp_areas = (comp_area_t*)realloc(
                comp_areas, sizeof(*comp_areas) * new_capacity);

    if (unlikely(!new_comp_areas))
        return false;

    comp_areas = new_comp_areas;
    comp_areas_capacity = new_capacity;

    return true;
}

static bool area_insert_at(comp_area_t *area, size_t index)
{
    if (unlikely(comp_areas_capacity == comp_areas_size)) {
        if (unlikely(!grow_areas()))
            return false;
    }

    if (unlikely(index < comp_areas_size)) {
        memmove(comp_areas + index + 1, comp_areas + index,
                sizeof(*comp_areas) * (comp_areas_size - index));
    }

    ++comp_areas_size;

    comp_areas[index] = *area;

    return true;
}

static void area_delete_at(size_t index)
{
    assert(index < comp_areas_size);

    --comp_areas_size;

    if (index < comp_areas_size) {
        memmove(comp_areas + index, comp_areas + (index + 1),
                sizeof(*comp_areas) * (comp_areas_size - index));
    }
}

static inline bool area_is_valid(comp_area_t *area)
{
    return area->ey > area->sy && area->ex > area->sx;
}

static bool surface_is_visible(surface_t *surface)
{
    for (size_t i = 0; i < comp_areas_size; ++i)
        if (comp_areas[i].surface == surface)
            return true;

    return false;
}

_always_inline
static int min(int a, int b)
{
    return a <= b ? a : b;
}

_always_inline
static int max(int a, int b)
{
    return a >= b ? a : b;
}

// Returns true if you should delete existing
static bool area_clip_pair(comp_area_t const *incoming,
                           comp_area_t const *existing)
{
    // Handle completely non-overlapping cases asap

    // Existing is below
    if (incoming->ey <= existing->sy)
        return false;

    // Existing is above
    if (incoming->sy >= existing->ey)
        return false;

    // Existing is to the left
    if (incoming->sx >= existing->ex)
        return false;

    // Existing is to the right
    if (incoming->ex <= existing->sx)
        return false;

    // top left incoming y
    int ay = incoming->sy;

    // top left incoming x
    int ax = incoming->sx;

    // bottom left incoming y
    int cy = incoming->ey;

    // top right incoming x
    int bx = incoming->ex;

    // top left existing y
    int ey = existing->sy;

    // top left existing x
    int ex = existing->sx;

    // bottom left existing y
    int sy = existing->ey;

    // top right existing x
    int fx = existing->ex;

    // top of middle portion
    int iy = max(ay, ey);

    // bottom of middle portion
    int ky = min(cy, sy);

    // width of left portion
    int l_w = ax - ex;

    // height of top portion
    int t_h = iy - ey;

    // width of center portion
    int c_w = bx - ax;

    // height of middle portion
    int m_h = ky - iy;

    // Calculate the offsets (u, v used for x, y image coord)
    int eu = existing->x;
    int ev = existing->y;

    // Middle starts after width of left portion
    int ju = eu + l_w;

    // Top starts after height of top portion
    int iv = ev + t_h;

    // Right starts center width after end of left portion
    int mu = ju + c_w;

    // bottom portion starts middle height after end of top portion
    int kv = iv + m_h;

    // area formed from top region
    comp_area_t efgh{ ey, ex, iy, fx, existing->surface, ev, eu };

    // area formed from left side of center region
    comp_area_t ijkl{ iy, ex, ky, ax, existing->surface, iv, eu };

    // area formed from right side of center region
    comp_area_t mnop{ iy, bx, ky, fx, existing->surface, iv, mu };

    // area formed by bottom region
    comp_area_t qrst{ ky, ex, sy, fx, existing->surface, kv, eu };

    if (area_is_valid(&efgh))
        area_insert_at(&efgh, comp_areas_size);

    if (area_is_valid(&ijkl))
        area_insert_at(&ijkl, comp_areas_size);

    if (area_is_valid(&mnop))
        area_insert_at(&mnop, comp_areas_size);

    if (area_is_valid(&qrst))
        area_insert_at(&qrst, comp_areas_size);

    return true;
}

static bool area_insert(comp_area_t *area)
{
    // Work backward from end, so inserted regions get ignored
    // Inserted regions couldn't possibly interfere with new area
    for (size_t i = comp_areas_size; i > 0; --i) {
        comp_area_t *victim = comp_areas + (i - 1);

        // If any clippings went in, delete the original
        if (area_clip_pair(area, victim))
            area_delete_at(i);
    }

    // Insert the new area
    return area_insert_at(area, comp_areas_size);
}

//
// Public API

int area_from_surface(surface_t *surface)
{
    //comp_area_t *p;
    return 0;
}

// Linked in font
extern bitmap_glyph_t const _binary_u_vga16_raw_start[];
extern bitmap_glyph_t const _binary_u_vga16_raw_end[];

static constexpr size_t const font_ascii_min = 32;
static constexpr size_t const font_ascii_max = 126;
static uint16_t font_ascii_lookup[1 + font_ascii_max - font_ascii_min];

static size_t font_glyph_count;
static char16_t const *font_glyph_codepoints;

static constexpr bitmap_glyph_t const * const font_glyphs =
        _binary_u_vga16_raw_start;

void surface_free(surface_t *pp)
{
    free(pp);
}

size_t font_glyph_index(size_t codepoint)
{
    if (likely(font_replacement &&
               codepoint >= font_ascii_min &&
               codepoint <= font_ascii_max))
        return font_ascii_lookup[codepoint - font_ascii_min];

    size_t st = 0;
    size_t en = font_glyph_count;
    size_t md;

    while (st < en) {
        md = ((en - st) >> 1) + st;
        char32_t gcp = font_glyph_codepoints[md];
        st = gcp <  codepoint ? md + 1 : st;
        en = gcp >= codepoint ? md : en;
    }

    if (unlikely(st >= font_glyph_count ||
                 font_glyph_codepoints[st] != codepoint))
        return size_t(-1);

    return st;
}

bitmap_glyph_t const *font_get_glyph(char32_t codepoint)
{
    size_t i = font_glyph_index(codepoint);

    return i != size_t(-1)
            ? &font_glyphs[i]
            : font_replacement
              ? &font_glyphs[font_replacement]
              : &font_glyphs[font_ascii_lookup[' ' - font_ascii_min]];
}

void font_init()
{
    font_glyph_count = (uintptr_t(_binary_u_vga16_raw_end) -
                   uintptr_t(_binary_u_vga16_raw_start)) /
            (sizeof(bitmap_glyph_t) + sizeof(uint16_t));

    font_glyph_codepoints = (char16_t*)(_binary_u_vga16_raw_start +
                                        font_glyph_count);

    size_t i = 0;
    for (bitmap_glyph_t const *it = _binary_u_vga16_raw_start;
         it != _binary_u_vga16_raw_end; ++i, ++it) {
        char16_t codepoint = font_glyph_codepoints[i];
        if (codepoint >= font_ascii_min && codepoint <= font_ascii_max)
            font_ascii_lookup[codepoint] = i;
    }

    // Lookup the unicode replacement character glyph
    font_replacement = font_glyph_index(0xFFFD);
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

#ifdef __x86_64__
#include <xmmintrin.h>
#endif

void vga_console_ring_t::bit8_to_pixels(uint32_t *out, uint8_t bitmap,
                                        uint32_t bg, uint32_t fg)
{
#ifdef __x86_64__
    __m128i map = _mm_set1_epi32(bitmap);

    __m128i lomask = _mm_set_epi32(0x8, 0x4, 0x2, 0x1);
    __m128i himask = _mm_slli_epi32(lomask, 4);

    lomask = _mm_and_si128(lomask, map);
    himask = _mm_and_si128(himask, map);

    lomask = _mm_cmpeq_epi32(lomask, _mm_setzero_si128());
    himask = _mm_cmpeq_epi32(himask, _mm_setzero_si128());

    __m128i fgs = _mm_set1_epi32(fg);
    __m128i bgs = _mm_set1_epi32(bg);

    lomask = _mm_or_si128(_mm_and_si128(bgs, lomask),
                          _mm_andnot_si128(fgs, lomask));
    himask = _mm_or_si128(_mm_and_si128(bgs, himask),
                          _mm_andnot_si128(fgs, himask));

    _mm_storeu_si128(reinterpret_cast<__m128i_u*>(out), lomask);
    _mm_storeu_si128(reinterpret_cast<__m128i_u*>(out + 4), himask);
#else
    out[0] = bitmap & 0x80 ? fg : bg;
    out[1] = bitmap & 0x40 ? fg : bg;
    out[2] = bitmap & 0x20 ? fg : bg;
    out[3] = bitmap & 0x10 ? fg : bg;
    out[4] = bitmap & 0x08 ? fg : bg;
    out[5] = bitmap & 0x04 ? fg : bg;
    out[6] = bitmap & 0x02 ? fg : bg;
    out[7] = bitmap & 0x01 ? fg : bg;
#endif
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

        while (size && cp < cp_max && cp < row_remain) {
            codepoints[cp++] = utf8_to_ucs4_upd(data);
            --size;
        }

        bitmap_glyph_t const *glyphs[cp_max];

        for (size_t i = 0; i < cp; ++i)
            glyphs[i] = font_get_glyph(codepoints[i]);

        // Draw glyph into surface

        uint32_t *row = pixels + width * font_h * out_y + font_w * out_x;

        // Doing loop interchange to write whole cache line of output across
        // Doing 0th scanline of each glyph, then 1st of each, then 2nd, etc

        for (size_t y = 0; y < 16; ++y, row += width) {
            // Scanline

            uint32_t *out = row;

            for (size_t i = 0; i < cp; ++i, out += font_w)
                bit8_to_pixels(out, glyphs[i]->bits[y], bg, fg);
        }

        out_x += cp;

        // If at end of row
        if (out_x == ring_w) {
            // Next row
            out_x = 0;
            ++out_y;

            if (out_y == ring_h) {
                // Past bottom of whole ring
                // Bump back into range
                --out_y;

                // Make a new row
                new_line(0);
            } else if (out_y == row_count) {
                // New row that has not been used yet
                new_line(0);
            }
        }
    }
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
