#include <surface.h>
#include <stdlib.h>
#include <string.h>
#include <sys/likely.h>
#include <new>
#include "../../user/include/utf.h"

__BEGIN_ANONYMOUS

#define CHARHEIGHT 16

struct bitmap_glyph_t {
    uint8_t bits[CHARHEIGHT];
};

class vga_font_t {
public:

    // Font dimensions in pixels
    static constexpr uint32_t font_w = 9;
    static constexpr uint32_t font_h = 16;

    using bit8_to_pixels_ptr = void (*)
        (uint32_t * restrict out, uint8_t bitmap, uint32_t bg, uint32_t fg);

    using bit8_to_pixels_transparent_ptr = void (*)
        (uint32_t * restrict out, uint8_t bitmap, uint32_t fg);

    static bit8_to_pixels_ptr bit8_to_pixels_resolve();

    static bit8_to_pixels_transparent_ptr bit8_to_pixels_transparent_resolve();

    // Function pointers point to best function for runtime CPU
    static bit8_to_pixels_ptr bit8_to_pixels;
    static bit8_to_pixels_transparent_ptr bit8_to_pixels_transparent;

    static void resolver();

    size_t glyph_index(size_t codepoint);
    bitmap_glyph_t const *get_glyph(char32_t codepoint);

private:
    static void bit8_to_pixels_generic(uint32_t * restrict out, uint8_t bitmap,
                                       uint32_t bg, uint32_t fg);

    static void bit8_to_pixels_sse(uint32_t * restrict out, uint8_t bitmap,
                                   uint32_t bg, uint32_t fg);

    static void bit8_to_pixels_avx2(uint32_t * restrict out, uint8_t bitmap,
                                    uint32_t bg, uint32_t fg);

    static void bit8_to_pixels_transparent_generic(
            uint32_t * restrict out, uint8_t bitmap, uint32_t fg);

    static void bit8_to_pixels_transparent_sse(
            uint32_t * restrict out, uint8_t bitmap, uint32_t fg);

    static void bit8_to_pixels_transparent_avx2(
            uint32_t * restrict out, uint8_t bitmap, uint32_t fg);
};

vga_font_t::bit8_to_pixels_ptr vga_font_t::bit8_to_pixels;

vga_font_t::bit8_to_pixels_transparent_ptr
    vga_font_t::bit8_to_pixels_transparent;

__attribute__((__constructor__))
static void vga_font_constructor()
{
    vga_font_t::resolver();
}

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
class vga_console_ring_t : public vga_console_t {
public:
    vga_console_ring_t(int32_t width, int32_t height);

    void reset() final;
    void new_line(uint32_t color) final;
    void write(char const *data, size_t size,
               uint32_t bg, uint32_t fg) final;
    void render(fb_info_t *fb, int dx, int dy, int dw, int dh) final;

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
        // row must be within ring_h rows of a valid range off either end
        assert(row + ring_h < ring_h * 3U);
        return row >= -ring_h
                ? row + ring_h
                : row >= ring_h
                  ? row - ring_h
                  : row;
    }

    // Clear the specified row, in surface space
    void clear_row(uint32_t row, uint32_t color);

private:

    // The place where writing text will go in surface space
    // This is advanced by writing text and newlines
    uint32_t out_x = 0, out_y = 0;

    // Size of ring in glyphs
    uint32_t ring_w, ring_h;

    vga_font_t font;

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
/// | |  I+ |---+  | |  + I  +  | |  +---| +I  | | +--|    |--+ |
/// | |c   d|m n|  | |c        d| |  |i j|c   d| | |ij| I  |mn| | clip top
/// | +-----+o p|  | +----------+ |  |k l+-----+ | |  |    |  | |
/// |     |q   r|  |    |q  r|    |  |q   r|     | |  |c  d|  | |
/// |     |  E  |  |    | E  |    |  |  E  |     | |kl+----+op| |
/// |     |s   t|  |    |s  t|    |  |s   t|     | |q   E    r| |
/// |     +-----+  |    +----+    |  +-----+     | |s        t| |
/// |             3|             2|             3| +----------+4|
/// +--------------+--------------+--------------+--------------+
/// |              |              |              |              |
/// | +-----+      | +----------+ |      +-----+ |    +----+    |
/// | |a   b|      | |a        b| |      |a   b| |    |a  b|    |
/// | |   + |---+  | |  +    +  | |  +---|  +  | | +--|    |--+ |
/// | |     |m n|  | |          | |  |i j|     | | |ij|    |mn| |
/// | |  I  | E |  | |    I     | |  | E |  I  | | |  | I  |  | | span entire
/// | |     |o p|  | |          | |  |k l|     | | | E|    |E | |
/// | |   + |---+  | |  +    +  | |  +---|  +  | | |  |    |  | |
/// | |c   d|      | |c        d| |      |c   d| | |kl|    |op| |
/// | +-----+      | +----------+ |      +-----+ | +--|c  d|--+ |
/// |             2| best case   1|             2|    +----+   3|
/// +--------------+--------------+--------------+--------------+
/// |              |              |              |              |
/// |    +------+  |    +----+    | +-------+    | +----------+ |
/// |    |e    f|  |    |e  f|    | |e     f|    | |e        f| |
/// |    |  E   |  |    | E  |    | |   E   |    | |    E     | |
/// |    |g    h|  |    |g  h|    | |g     h|    | |g        h| |
/// | +-----+m n|  | +----------+ | |i   j+----+ | |ij+----+mn| | clip bottom
/// | |a   b|   |  | |a        b| | |k   l|a  b| | |kl|a  b|op| |
/// | |  I  |o p|  | |  + I  +  | | +-----| +I | | +--| I  |--+ |
/// | |c + d|---+  | |c        d| |       |c  d| |    |c  d|    |
/// | +-----+      | +----------+ |       +----+ |    +----+    |
/// |             3|             2|             3|             4|
/// +--------------+--------------+--------------+--------------+
/// |    +------+  |   +------+   |    +-----+   | +----------+ |
/// |    |e    f|  |   |e    f|   |    |e   f|   | |e        f| |
/// |    |g    h|  |   |g E  h|   |    |g   h|   | |g        h| |
/// | +-----+m n|  | +----------+ |    |i j+---+ | |ij+----+mn| |
/// | |a   b|   |  | |a        b| |    |   |a b| | |  |a  b|  | |
/// | |  I  | E |  | |    I     | |    | E | I | | |E | I  |  | | within
/// | |c   d|   |  | |c        d| |    |   |c d| | |  |c  d|  | |
/// | +-----+o p|  | +----------+ |    |k l+---+ | |kl+----+op| |
/// |    |q    r|  |   |q E  r|   |    |q   r|   | |q        r| |
/// |    |s    t|  |   |s    t|   |    |s   t|   | |s        t| |
/// |    +------+  |   +------+   |    +-----+   | +----------+ |
/// |             4|             3|             4| worst case  5|
/// +--------------+--------------+--------------+--------------+
///
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

__END_ANONYMOUS

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

static size_t font_glyph_index(size_t codepoint)
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

static bitmap_glyph_t const *font_get_glyph(char32_t codepoint)
{
    size_t i = font_glyph_index(codepoint);

    return i != size_t(-1)
            ? &font_glyphs[i]
            : font_replacement
              ? &font_glyphs[font_replacement]
              : &font_glyphs[font_ascii_lookup[' ' - font_ascii_min]];
}

static void font_init()
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
    : vga_console_t(width, height, (uint32_t*)(this + 1))
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

vga_font_t::bit8_to_pixels_ptr vga_font_t::bit8_to_pixels_resolve()
{
    return __builtin_cpu_supports("avx2")
            ? bit8_to_pixels_avx2
            : __builtin_cpu_supports("sse2")
              ? bit8_to_pixels_sse
              : bit8_to_pixels_generic;
}

vga_font_t::bit8_to_pixels_transparent_ptr
vga_font_t::bit8_to_pixels_transparent_resolve()
{
    return __builtin_cpu_supports("avx2")
            ? &vga_font_t::bit8_to_pixels_transparent_avx2
            : __builtin_cpu_supports("sse2")
              ? &vga_font_t::bit8_to_pixels_transparent_sse
              : &vga_font_t::bit8_to_pixels_transparent_generic;
}

void vga_font_t::resolver()
{
    bit8_to_pixels_resolve();
    bit8_to_pixels_transparent_resolve();
}

//void vga_console_ring_t::render(fb_info_t *fb, int dx, int dy, int dw, int dh)
//{
//    // The scroll offset may cause us to need to simultaneously
//    // show a bit of the bottom, and a bit of the top of the surface

//    comp_area_t top_area;
//    comp_area_t bot_area;

//    // Fill simple values
//    top_area.sx = dx;
//    top_area.sy = dy;
//    top_area.ex = dx + dw;

//    bot_area.sx = dx;
//    bot_area.ex = dx + dw;
//    bot_area.ey = dy + dh;

//    //bot_area.sy = dy;

//    // Row index of row at top of viewport
//    uint32_t first_visible = wrap_row(oldest_row + scroll_top);

//    // Row index of row at bottom of viewport
////    uint32_t last_visible = wrap_row(first_visible + )

////    // If the oldest row
////    if (wrap_row_signed(oldest_row + scroll_top)
//}

void vga_console_ring_t::clear_row(uint32_t row, uint32_t color)
{
    assert(row < ring_h);
    uint32_t sx = 0;
    uint32_t sy = row * font.font_h;
    uint32_t ex = ring_w * font.font_w;
    uint32_t ey = sy + font.font_h;

    fill(sx, sy, ex, ey, color);
}

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

void vga_font_t::bit8_to_pixels_generic(
        uint32_t * restrict out, uint8_t bitmap,
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

void vga_font_t::bit8_to_pixels_transparent_generic(
        uint32_t * restrict out, uint8_t bitmap,
        uint32_t fg)
{
    out[0] = bitmap & 0x80 ? fg : out[0];
    out[1] = bitmap & 0x40 ? fg : out[1];
    out[2] = bitmap & 0x20 ? fg : out[2];
    out[3] = bitmap & 0x10 ? fg : out[3];
    out[4] = bitmap & 0x08 ? fg : out[4];
    out[5] = bitmap & 0x04 ? fg : out[5];
    out[6] = bitmap & 0x02 ? fg : out[6];
    out[7] = bitmap & 0x01 ? fg : out[7];
}

__attribute__((__target__("sse2")))
void vga_font_t::bit8_to_pixels_sse(
        uint32_t * restrict out, uint8_t bitmap,
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

__attribute__((__target__("avx2")))
void vga_font_t::bit8_to_pixels_avx2(
        uint32_t * restrict out, uint8_t bitmap, uint32_t fg, uint32_t bg)
{
    // Load bitmap into all lanes
    __m256i map = _mm256_set1_epi32(bitmap);

    // Generate mask that checks one bit in each lane
    __m256i mask = _mm256_set_epi32(0x80, 0x40, 0x20, 0x10,
                                    0x08, 0x04, 0x02, 0x01);

    // Make a value that is not zero if the bit is set for this lane
    mask = _mm256_and_si256(mask, map);

    // Make a mask that is all ones if the bitmap bit is set for this lane
    mask = _mm256_cmpeq_epi32(mask, _mm256_setzero_si256());

    // Set foreground color in all lanes
    __m256i fgs = _mm256_set1_epi32(fg);

    // Set background color in all lanes
    __m256i bgs = _mm256_set1_epi32(bg);

    // Select either background or foreground depending on whether
    // the bitmap bit for this lane was set
    mask = _mm256_or_si256(_mm256_and_si256(bgs, mask),
                          _mm256_andnot_si256(fgs, mask));

    // Store 8 pixels
    _mm256_storeu_si256(reinterpret_cast<__m256i_u*>(out), mask);
}

__attribute__((__target__("sse2")))
void vga_font_t::bit8_to_pixels_transparent_sse(
        uint32_t * restrict out, uint8_t bitmap, uint32_t fg)
{
    // Load existing pixels for transparency merge
    __m128i lobg = _mm_loadu_si128(reinterpret_cast<__m128i_u*>(out));
    __m128i hibg = _mm_loadu_si128(reinterpret_cast<__m128i_u*>(out + 4));

    // Load bitmap into all lanes
    __m128i map = _mm_set1_epi32(bitmap);

    // Generate mask that checks one bit in each lane
    __m128i lomask = _mm_set_epi32(0x8, 0x4, 0x2, 0x1);
    __m128i himask = _mm_slli_epi32(lomask, 4);

    // Make a value that is not zero if the bit is set for this lane
    lomask = _mm_and_si128(lomask, map);
    himask = _mm_and_si128(himask, map);

    // Make a mask that is all ones if the bitmap bit is set for this lane
    __m128i zero = _mm_setzero_si128();
    lomask = _mm_cmpeq_epi32(lomask, zero);
    himask = _mm_cmpeq_epi32(himask, zero);

    // Set foreground color in all lanes
    __m128i fgs = _mm_set1_epi32(fg);

    // Select either background or foreground depending on whether
    // the bitmap bit for this lane was set
    lomask = _mm_or_si128(_mm_and_si128(lobg, lomask),
                          _mm_andnot_si128(fgs, lomask));
    // Store low pixels
    _mm_storeu_si128(reinterpret_cast<__m128i_u*>(out), lomask);

    himask = _mm_or_si128(_mm_and_si128(hibg, himask),
                          _mm_andnot_si128(fgs, himask));
    _mm_storeu_si128(reinterpret_cast<__m128i_u*>(out + 4), himask);
}

__attribute__((__target__("avx2")))
void vga_font_t::bit8_to_pixels_transparent_avx2(
        uint32_t * restrict out, uint8_t bitmap, uint32_t fg)
{
    // Load existing pixels for transparency merge
    __m256i bg = _mm256_loadu_si256(reinterpret_cast<__m256i_u*>(out));

    // Load bitmap into all lanes
    __m256i map = _mm256_set1_epi32(bitmap);

    // Generate mask that checks one bit in each lane
    __m256i mask = _mm256_set_epi32(0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01);

    // Make a value that is not zero if the bit is set for this lane
    mask = _mm256_and_si256(mask, map);

    // Make a mask that is all ones if the bitmap bit is set for this lane
    __m256i zero = _mm256_setzero_si256();
    mask = _mm256_cmpeq_epi32(mask, zero);

    // Set foreground color in all lanes
    __m256i fgs = _mm256_set1_epi32(fg);

    // Select either background or foreground depending on whether
    // the bitmap bit for this lane was set
    mask = _mm256_or_si256(_mm256_and_si256(bg, mask),
                          _mm256_andnot_si256(fgs, mask));

    // Store 8 pixels
    _mm256_storeu_si256(reinterpret_cast<__m256i_u*>(out), mask);
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

        uint32_t *row = pixels +
                width * font.font_h * out_y +
                font.font_w * out_x;

        // Doing loop interchange to write whole cache line of output across
        // Doing 0th scanline of each glyph, then 1st of each, then 2nd, etc

        for (size_t y = 0; y < font.font_h; ++y, row += width) {
            // Scanline

            uint32_t *out = row;

            for (size_t i = 0; i < cp; ++i, out += font.font_w)
                vga_font_t::bit8_to_pixels(out, glyphs[i]->bits[y], bg, fg);
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

//1 (adds 0 rect) 1 case 6.25%
//2 (adds 1 rect) 4 case 25%
//3 (adds 2 rect) 6 case 37.5%
//4 (adds 3 rect) 4 case 25%
//5 (adds 4 rect) 1 case 6.25%
//
// (adds 2 rect) 37.5%
// (adds 1 rect) 25%
// (adds 3 rect) 25%
// (adds 0 rect) 6.25%
// (adds 4 rect) 6.25%

vga_console_t *vga_console_factory_t::create(int32_t w, int32_t h)
{
    vga_console_ring_t *console = new (w * 9, h * 16)
            vga_console_ring_t(w * 9, h * 16);
    return console;
}

void vga_console_ring_t::render(fb_info_t *fb, int dx, int dy, int dw, int dh)
{

}
