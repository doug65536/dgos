#include "assert.h"
#include "vesainfo.h"
#include "export.h"
#if 0
#include "framebuffer.h"
#include "thread.h"
#include "mm.h"
#include "string.h"
#include "likely.h"
#include "math.h"
#include "printk.h"
#include "utility.h"
#include "bootinfo.h"
#include "dev_text.h"

#define CHARHEIGHT 16

struct pix_fmt_t {
    uint8_t mask_size_r;
    uint8_t mask_size_g;
    uint8_t mask_size_b;
    uint8_t mask_size_a;
    uint8_t mask_pos_r;
    uint8_t mask_pos_g;
    uint8_t mask_pos_b;
    uint8_t mask_pos_a;
};

// The values are for little endian machine
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
// The letters in the name are byte order
static const pix_fmt_t fb_rgba32{8, 8, 8, 8,   0, 8, 16, 24};
static const pix_fmt_t fb_bgra32{8, 8, 8, 8,   16, 8, 0, 24};

static const pix_fmt_t fb_rgb24{8, 8, 8, 0,   0, 8, 16, 0};
static const pix_fmt_t fb_bgr24{8, 8, 8, 0,   16, 8, 0, 0};

static const pix_fmt_t fb_rgb565{5, 6, 5, 0,   0, 5, 11, 0};
static const pix_fmt_t fb_rgb555{5, 5, 5, 0,   0, 5, 10, 0};
#else
// The letters in the name are opposite of byte order
static constexpr const pix_fmt_t fb_bgra32{8, 8, 8, 8,   8, 16, 24, 0};
static constexpr const pix_fmt_t fb_rgba32{8, 8, 8, 8,   24, 16, 8, 0};

static constexpr const pix_fmt_t fb_rgb24{8, 8, 8, 0,   16, 8, 0, 0};
static constexpr const pix_fmt_t fb_bgr24{8, 8, 8, 0,   0, 8, 16, 0};

static constexpr const pix_fmt_t fb_rgb565{5, 6, 5, 0,   11, 5, 0, 0};
static constexpr const pix_fmt_t fb_rgb555{5, 5, 5, 0,   10, 5, 0, 0};
#endif

struct surface_t {
    rect_t area;
    int pitch;

    uint8_t *backing;

    uint8_t bpp, byte_pp;
    pix_fmt_t pix_fmt;

    surface_t map_noclip(rect_t region);
    surface_t map(rect_t region);
    void clear(uint32_t color);
    void fill_rect(rect_t rect, uint32_t color);
    void move_rect(rect_t dst, vec2_t src);
    void draw_chars(vec2_t pos, char32_t *str, size_t len);
    void blt(rect_t dst, vec2_t src, surface_t *src_surface);
};

struct fb_coord_t {
    int x;
    int y;
};

struct fb_rect_t {
    fb_coord_t st;
    fb_coord_t en;
};

struct framebuffer_t {
    uint8_t *video_mem;
    uint8_t *backing;
    vbe_selected_mode_t mode;
    fb_rect_t dirty;
};

static framebuffer_t fb;

struct bitmap_glyph_t {
    uint8_t bits[CHARHEIGHT];
};

bitmap_glyph_t const *get_glyph(size_t codepoint);

// Linked in font
extern bitmap_glyph_t const _binary_u_vga16_raw_start[];
extern bitmap_glyph_t const _binary_u_vga16_raw_end[];

#if 0
class framebuffer_console_factory_t
        : public text_dev_factory_t
        , public zero_init_t {
public:
    constexpr framebuffer_console_factory_t()
        : text_dev_factory_t("framebuffer_console")
    {
    }

    int detect(text_dev_base_t ***ptrs) override final;
};

static framebuffer_console_factory_t framebuffer_console_factory;

class framebuffer_console_t : public text_dev_base_t {
public:
    framebuffer_console_t()
    {
        screen = (void*)fb.video_mem;
    }

    static size_t glyph_index(size_t codepoint);
    static bitmap_glyph_t const *glyph(size_t codepoint);

    static const constexpr size_t font_width = 9;
    static const constexpr size_t font_height = 16;

private:
    // text_dev_base_t interface
    TEXT_DEV_IMPL

    // The offscreen back buffer copy of the frame bitmap
    void *shadow = nullptr;

    void *screen = nullptr;

    // The onscreen mapping of the frame bitmap
    //void *shadow;

    friend class framebuffer_console_factory_t;

    static void static_init();

    void fill_region(int sx, int sy, int ex, int ey, int character);

    void move_cursor_if_on();
    void move_cursor_to(int x, int y);

    void cap_position(int *px, int *py);
    void advance_cursor(int distance);
    void next_line();

    int cursor_x = 0;
    int cursor_y = 0;
    int cursor_on = 0;

    int width = 0;
    int height = 0;

    int ofs_x = 0;
    int ofs_y = 0;

    int fg_color = 0x123456;
    int bg_color = 0;

    int mouse_x = 0;
    int mouse_y = 0;
    int mouse_on = 0;

    static constexpr bitmap_glyph_t const * const glyphs =
            _binary_u_vga16_raw_start;

    // O(1) glyph lookup for ASCII 32-126 range
    static constexpr size_t const ascii_min = 32;
    static constexpr size_t const ascii_max = 126;
    static size_t glyph_count;
    static uint16_t replacement;
    static uint16_t *glyph_codepoints;
    static uint8_t ascii_lookup[1 + ascii_max - ascii_min];

    static framebuffer_console_t instances[1];
    static unsigned instance_count;

    void scroll_screen(int x, int y);
    void clear_screen();
};

framebuffer_console_t framebuffer_console_t::instances[1];
unsigned framebuffer_console_t::instance_count;
uint8_t framebuffer_console_t::ascii_lookup[1 + ascii_max - ascii_min];
uint16_t framebuffer_console_t::replacement;
size_t framebuffer_console_t::glyph_count;
uint16_t *framebuffer_console_t::glyph_codepoints;
#endif// framebuffer_console

size_t glyph_count;
uint16_t *glyph_codepoints;
static constexpr size_t const ascii_min = 32;
static constexpr size_t const ascii_max = 126;
uint8_t ascii_lookup[1 + ascii_max - ascii_min];
size_t glyph_index(size_t codepoint);
uint16_t replacement;

#define USE_NONTEMPORAL 0

static _always_inline void fb_reset_dirty(void)
{
    fb.dirty.st.x = fb.mode.width;
    fb.dirty.st.y = fb.mode.height;
    fb.dirty.en.x = 0;
    fb.dirty.en.y = 0;
}

void fb_init(void)
{
    vbe_selected_mode_t *mode_info = (vbe_selected_mode_t*)
            bootinfo_parameter(bootparam_t::vbe_mode_info);

    if (!mode_info)
        return;

    fb.mode = *mode_info;

    size_t screen_size = fb.mode.pitch * fb.mode.height * sizeof(uint32_t);

    // Round the back buffer size up to a multiple of the cache line size
    screen_size = (screen_size + 63) & -64;

    // This is really early on, too early to allocate a huge back buffer
    // Just point both at the hardware video memory
    fb.backing = (uint8_t*)fb.mode.framebuffer_addr;
    fb.video_mem = (uint8_t*)fb.mode.framebuffer_addr;

    fb_reset_dirty();
}

static _always_inline void fb_update_dirty(
        int left, int top, int right, int bottom)
{
    fb.dirty.en.x = (fb.dirty.en.x < right ? right : fb.dirty.en.x);
    fb.dirty.en.y = (fb.dirty.en.y < bottom ? bottom : fb.dirty.en.y);
    fb.dirty.st.x = (fb.dirty.st.x > left ? left : fb.dirty.st.x);
    fb.dirty.st.y = (fb.dirty.st.y > top ? top : fb.dirty.st.y);
}

void fb_copy_to(int scr_x, int scr_y,
                int img_pitch, int img_w, int img_h,
                uint32_t const *pixels)
{
    if (unlikely(scr_x >= fb.mode.width)) {
        // Offscreen right
        return;
    }

    if (unlikely(scr_x + img_w <= 0)) {
        // Offscreen left
        return;
    }

    if (unlikely(scr_y >= fb.mode.height)) {
        // Offscreen bottom
        return;
    }

    if (unlikely(scr_y + img_h <= 0)) {
        // Offscreen top
        return;
    }

    if (unlikely(scr_x + img_w > fb.mode.width)) {
        // Right-clipped
        img_w = fb.mode.width - scr_x;
    }

    if (unlikely(scr_y + img_h > fb.mode.height)) {
        // Bottom-clipped
        img_h = fb.mode.height - scr_y;
    }

    if (unlikely(scr_x < 0)) {
        // Left-clipped
        pixels -= scr_x;
        img_w += scr_x;
        scr_x = 0;
    }

    if (unlikely(scr_y < 0)) {
        // Top-clipped
        pixels -= scr_y * img_pitch;
        img_h += scr_y;
        scr_y = 0;
    }

    int scr_ex = scr_x + img_w;
    int scr_ey = scr_y + img_h;

    fb_update_dirty(scr_x, scr_y, scr_ex, scr_ey);

    uint8_t *out = fb.backing +
            (scr_y * fb.mode.pitch + scr_x * sizeof(uint32_t));

    for (int y = scr_y; y < scr_ey; ++y, out += fb.mode.pitch) {
#if USE_NONTEMPORAL
        memcpy32_nt(out, pixels, img_w * sizeof(uint32_t));
#else
        memcpy(out, pixels, img_w * sizeof(uint32_t));
#endif

        pixels += img_pitch;
    }
}

static void nullify_degenerate(fb_rect_t& rect)
{
    // Clamp end against start
    rect.en.x = ext::max(rect.en.x, rect.st.x);
    rect.en.y = ext::max(rect.en.y, rect.st.y);
}

static void scissor_rect(fb_rect_t& rect, fb_rect_t const& scissor)
{
    // Clamp start against left and top edge
    rect.st.x = ext::max(rect.st.x, scissor.st.x);
    rect.st.y = ext::max(rect.st.x, scissor.st.y);

    // Clamp end against right and bottom edge
    rect.en.x = ext::min(rect.en.x, scissor.en.x);
    rect.en.y = ext::min(rect.en.y, scissor.en.y);
}

void fb_fill_rect_clamped(int sx _unused, int sy _unused,
                          int ex _unused, int ey _unused,
                          uint32_t color _unused)
{

}

void fb_fill_rect(int sx, int sy, int ex, int ey, uint32_t color)
{
    fb_update_dirty(sx, sy, ex, ey);

    size_t row_ofs = sx * sizeof(uint32_t);
    size_t row_end = ex * sizeof(uint32_t);

    uint8_t *out = fb.backing + sy * fb.mode.pitch + row_ofs;
    size_t width = row_end - row_ofs;
    for (int y = sy; y < ey; ++y) {
        memset32_nt(out, color, width);
        out += fb.mode.pitch;
    }
}

#if 1
template<typename F>
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color, F setpixel)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = ext::max(dx, -dx);
    int ady = ext::max(dy, -dy);

    if (adx >= ady) {
        // Shallow, x changes more than y

        int d = 2 * ady - adx;
        int s = y0;
        int fa = x0 <= x1 ? 1 : -1;
        int sa = y0 <= y1 ? 1 : -1;

        for (int f = x0; f != x1; f += fa) {
            setpixel(f, s, color);
            if (d > 0) {
               s = s + sa;
               d = d - 2 * adx;
            }
            d = d + 2 * ady;
        }
    } else {
        // Steep, y changes more than x
        int d = 2 * adx - ady;
        int s = x0;
        int fa = y0 <= y1 ? 1 : -1;
        int sa = x0 <= x1 ? 1 : -1;

        for (int f = y0; f != y1; f += fa) {
            setpixel(s, f, color);
            if (d > 0) {
               s = s + sa;
               d = d - 2 * ady;
            }
            d = d + 2 * adx;
        }
    }
}

void fb_draw_aa_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    fb_draw_line(x0, y0, x1, y1, color, [](int x, int y, uint32_t pixel_color) {
        if (x >= 0 && x < fb.mode.width && y >= 0 && y < fb.mode.height)
            ((uint32_t*)(fb.backing + fb.mode.pitch * y))[x] = pixel_color;
    });
}

#else
void fb_draw_aa_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    // fixme: clip properly
    if (x0 < 0 || x1 < 0)
        return;

    fb_update_dirty(x0, y0, x1, y1);

    __f32_vec4 fcolor = {
        float(color & 0xFF),
        float((color >> 8) & 0xFF),
        float((color >> 16) & 0xFF),
        float((color >> 24) & 0xFF)
    };

    uint8_t *addr = fb.back_buf + (y0 * fb.mode.pitch + x0 * sizeof(uint32_t));

    // x and y delta
    int dx = x1 - x0;
    int dy = y1 - y0;

    int du, dv, u, v, uincr, vincr;

    // absolute delta
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    if (abs_dx > abs_dy)
    {
        // More horizontal than vertical
        du = abs_dx;
        dv = abs_dy;
        u = x1;
        v = y1;
        uincr = sizeof(uint32_t);
        vincr = fb.mode.pitch;
        if (dx < 0)
            uincr = -uincr;
        if (dy < 0)
            vincr = -vincr;
    }
    else
    {
        // More vertical than horizontal
        du = abs_dy;
        dv = abs_dx;
        u = y1;
        v = x1;
        uincr = fb.mode.pitch;
        vincr = sizeof(uint32_t);
        if (dy < 0)
            uincr = -uincr;
        if (dx < 0)
            vincr = -vincr;
    }

    int uend = u + 2 * du;
    int d = (dv + dv) - du;
    int incrS = 2 * dv;
    int incrD = 2 * (dv - du);
    int twovdu = 0;
    float fdu = (float)du;
    float fdv = (float)dv;
    float invD = 1.0f / (2.0f * sqrtf(fdu*fdu + fdv*fdv));
    float invD2du = 2.0f * (du * invD);

    do
    {
        fb_blend_pixel(addr - vincr, fcolor, (invD2du + twovdu*invD) * (1.0f/1.5f) + 0.001f);
        fb_blend_pixel(addr,         fcolor, (          twovdu*invD) * (1.0f/1.5f) + 0.001f);
        fb_blend_pixel(addr + vincr, fcolor, (invD2du - twovdu*invD) * (1.0f/1.5f) + 0.001f);

        if (d < 0)
        {
            // u
            twovdu = d + du;
            d += incrS;
        }
        else
        {
            // u+v
            twovdu = d - du;
            d += incrD;
            ++v;
            addr += vincr;
        }

        ++u;
        addr += uincr;
    } while (u < uend);
}

void fb_clip_aa_line(int x0, int y0, int x1, int y1)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
}
#endif

static void fb_update_vidmem(int left, int top, int right, int bottom)
{
    assert(left >= 0);
    assert(top >= 0);
    assert(right <= fb.mode.width);
    assert(bottom <= fb.mode.height);

    size_t width = (right - left) * sizeof(uint32_t);
    size_t row_ofs = top * fb.mode.pitch + left * sizeof(uint32_t);

    for (int y = top; y < bottom; ++y, row_ofs += fb.mode.pitch) {
        // Calculate the address of the first pixel
        size_t ofs = row_ofs;

        // Align the start to the beginning of a cache line
        size_t misalignment = ofs & 63;
        ofs -= misalignment;

        // Increase the width by the alignment fixup
        size_t copy_width = width + misalignment;

        // Expand the copy size to a full cache line
        copy_width = (copy_width + 63) & -64;

        // Copy whole cache lines without polluting cache
#if USE_NONTEMPORAL
        memcpy512_nt(fb.video_mem + ofs, fb.back_buf + ofs, copy_width);
#else
        memcpy(fb.video_mem + ofs, fb.backing + ofs, copy_width);
#endif
    }
#if USE_NONTEMPORAL
    memcpy_nt_fence();
#endif
}

void fb_update(void)
{
    fb_update_vidmem(fb.dirty.st.x, fb.dirty.st.y,
                     fb.dirty.en.x, fb.dirty.en.y);
    fb_reset_dirty();
}

///////////////////

// Transform [7:0]=R [15:8]=G [23:16}=B into device specific pixel format
static uint32_t rgb(uint32_t color)
{
    unsigned r = color & 0xFF;
    unsigned g = (color >> 8) & 0xFF;
    unsigned b = (color >> 16) & 0xFF;
    r >>= 8 - fb.mode.mask_size_r;
    g >>= 8 - fb.mode.mask_size_g;
    b >>= 8 - fb.mode.mask_size_b;
    return (r << fb.mode.mask_pos_r) |
            (g << fb.mode.mask_pos_g) |
            (b << fb.mode.mask_pos_b);
}

static void fill_rect(int sx, int sy, int ex, int ey, uint32_t color)
{
    if (unlikely(!fb.video_mem))
        return;

    // Clamp to right edge
    if (unlikely(ex > fb.mode.width))
        ex = fb.mode.width;

    // Clamp to bottom edge
    if (unlikely(ey > fb.mode.height))
        ey = fb.mode.height;

    // Clamp to left edge
    if (unlikely(sx < 0))
        sx = 0;

    // Clamp to top edge
    if (unlikely(sy < 0))
        sy = 0;

    // Reject degenerate
    if (unlikely(ex <= sx))
        return;

    // Reject degenerate
    if (unlikely(ey <= sy))
        return;

    uint32_t pixel_color = rgb(color);

    char *dest = (char*)(fb.video_mem +
                         sy * fb.mode.pitch +
                         sx * fb.mode.byte_pp);
    size_t wid = (ex - sx);
    size_t skip = fb.mode.pitch - (wid * fb.mode.byte_pp);

    switch (fb.mode.byte_pp) {
    case 4:
        while (sy < ey) {
            memset32(dest, pixel_color, wid);
            dest += skip;
            ++sy;
        }
        break;

    case 3:
        // Take advantage of 4 pixels every 3 dwords
        /// a = Rbgr
        /// b = grBG
        /// c = BGRb
        /// possible misalignments are
        /// 3, 2, 1, which is also the number of pixels
        /// remaining until it is dword aligned again
        pixel_color &= 0xFFFFFF;

        size_t misalign_st;
        size_t misalign_en;
        misalign_st = uintptr_t(dest) & 3;
        misalign_en = (uintptr_t(dest) + (wid * 3)) & 3;

        // Width of left byte-filled area
        int left_partial;
        left_partial = misalign_st;

        // Width of right byte-filled area
        int right_partial;
        right_partial = (misalign_en ? 4 - misalign_en : 0);

        sx += left_partial;
        ex -= left_partial;

        size_t left_sz;
        size_t right_sz;
        left_sz = 3 * left_partial;
        right_sz = 3 * right_partial;

        uint32_t grp[3];
        grp[0] = pixel_color | (pixel_color << 24);
        grp[1] = (pixel_color << 16) | ((pixel_color >> 8) & 0xFFFF);
        grp[2] = (pixel_color << 8) | (pixel_color >> 16);

        while (sy < ey) {
            if (left_sz)
                memcpy(dest, grp, left_sz);

            for (int x = sx; x + 3 < ex; ++x) {
                *((uint32_t*)dest++) = grp[0];
                *((uint32_t*)dest++) = grp[1];
                *((uint32_t*)dest++) = grp[2];
            }

            if (right_sz)
                memcpy(dest, grp, right_sz);

            dest += skip;
            ++sy;
        }
        break;

    case 1:
        while (sy < ey) {
            memset8(dest, pixel_color, wid);
            dest += skip;
            ++sy;
        }
        break;

    case 2:
        while (sy < ey) {
            memset16(dest, pixel_color, wid);
            dest += skip;
            ++sy;
        }
        break;

    }
}

static void blt_rect_noclip(surface_t *dst, int dx, int dy,
                            surface_t *src, int dw, int dh,
                            int sx, int sy)
{
    int dxe = dx + dw;
    int dye = dy + dh;
    int sxe = sx + dw;
    int sye = sx + dh;


}

static void scroll_rect(int sx _unused, int sy _unused,
                        int ex _unused, int ey _unused,
                        int dx _unused, int dy _unused)
{
}

static void fb_draw_char(vec2_t pos, bitmap_glyph_t const * restrict glyph,
                         uint32_t fg, uint32_t bg)
{
    if (unlikely(unsigned(pos.x + 8) > fb.mode.width ||
                 unsigned(pos.y + CHARHEIGHT) > fb.mode.height))
        return;

    // Simple, unclipped...

    bool glyph_pixels[8*CHARHEIGHT];

    for (size_t y = 0, i = 0; y < CHARHEIGHT; ++y) {
        uint8_t scan = glyph->bits[y];
        for (size_t x = 0; x < 8; ++x) {
            glyph_pixels[i++] = (int8_t(scan) < 0);
            scan <<= 1;
        }
    }

    size_t sx = pos.x;
    size_t sy = pos.y;
    size_t ex = pos.x + 8;
    size_t ey = pos.y + CHARHEIGHT;

    fg = rgb(fg);
    bg = rgb(bg);

    if (likely(unsigned(ex) <= fb.mode.width &&
               unsigned(ey) <= fb.mode.height)) {
        uint8_t *dest = fb.video_mem +
                sy * fb.mode.pitch +
                sx * fb.mode.byte_pp;
        size_t skip = fb.mode.pitch - (8 * fb.mode.byte_pp);

        switch (fb.mode.byte_pp) {
        case 4:
            for (size_t y = sy, i = 0; y < ey; ++y) {
                for (size_t x = sx; x < ex; ++x) {
                    *(uint32_t*)dest = glyph_pixels[i++] ? fg : bg;
                    dest += sizeof(uint32_t);
                }
                dest += skip;
            }
            break;
        case 3:
            for (size_t y = sy, i = 0; y < ey; ++y) {
                for (size_t x = sx; x < ex; ++x) {
                    uint32_t c = glyph_pixels[i++] ? fg : bg;
                    *dest++ = c & 0xFF;
                    *dest++ = (c >> 8) & 0xFF;
                    *dest++ = (c >> 16) & 0xFF;
                }
                dest += skip;
            }
            break;
        case 2:
            for (size_t y = sy, i = 0; y < ey; ++y) {
                for (size_t x = sx; x < ex; ++x) {
                    uint32_t c = glyph_pixels[i++] ? fg : bg;
                    *(uint16_t*)dest = c;
                    dest += sizeof(uint16_t);
                }
                dest += skip;
            }
            break;
        }
    }
}

void fb_draw_char(vec2_t pos, char32_t codepoint, uint32_t fg, uint32_t bg)
{
    bitmap_glyph_t const *glyph = get_glyph(codepoint);
    fb_draw_char(pos, glyph, fg, bg);
}

void scroll_up(size_t dist, uint32_t bg)
{
    if (fb.mode.pitch == fb.mode.width * fb.mode.byte_pp) {
        uint8_t *src = fb.video_mem + dist * fb.mode.pitch;
        uint8_t *dst = fb.video_mem;
        memmove(dst, src, fb.mode.pitch *
                (fb.mode.height - dist));
    } else {
        uint8_t *src = fb.video_mem + dist * fb.mode.pitch;
        uint8_t *dst = fb.video_mem;
        for (size_t i = 0; i < dist; ++i) {
            memmove(dst, src, fb.mode.pitch * fb.mode.width);
            src += fb.mode.pitch;
            dst += fb.mode.pitch;
        }
    }
    fill_rect(0, fb.mode.height - dist,
              fb.mode.width, fb.mode.height, bg);
}

static size_t draw_chars(surface_t *s, vec2_t pos,
                     char const *str,
                     uint32_t fg, uint32_t bg)
{
    if (unlikely(!s->area.is_inside(pos)))
        return 0;

    int se = s->area.en.x;
    char const *orig_str = str;
    while (char32_t codepoint = utf8_to_ucs4_upd(str) && pos.x < se) {
        fb_draw_char(pos, codepoint, fg, bg);

        pos.x += 9;
    }

    return str - orig_str;
}

static void draw_str(vec2_t pos, char const* str,
                     uint32_t fg, uint32_t bg)
{
    while (char32_t codepoint = utf8_to_ucs4_upd(str)) {
        auto glyph = get_glyph(codepoint);
        fb_draw_char(pos, glyph, fg, bg);
        pos.x += 9;

        if (unlikely(pos.x + 9 > fb.mode.width)) {
            // Went off the right side
            pos.x = 0;
            if (unlikely(pos.y + CHARHEIGHT > fb.mode.height)) {
                size_t dist = pos.y + CHARHEIGHT - fb.mode.height;

                scroll_up(dist, bg);
                pos.y = fb.mode.height - CHARHEIGHT;
            } else {
                pos.y += CHARHEIGHT;
            }
        }
    }
}

///

_constructor(ctor_ctors_ran)
void fb_early_init()
{
    // Detect the number of items from size
    auto en = uintptr_t(_binary_u_vga16_raw_end);
    auto st = uintptr_t(_binary_u_vga16_raw_start);
    auto sz = en - st;
    glyph_count = sz / (sizeof(bitmap_glyph_t) + sizeof(uint16_t));
    glyph_codepoints = (uint16_t*)(_binary_u_vga16_raw_start + glyph_count);

    // Populate ASCII glyph lookup table
    for (size_t i = ascii_min; i <= ascii_max; ++i)
        ascii_lookup[i - ascii_min] = glyph_index(i);

//    instances[0].fg_color = 0xe0c0d0;

    // Lookup unicode replacement character
    replacement = glyph_index(0xFFFD);
}

void move_cursor_if_on()
{
    if (cursor_on)
        move_cursor_to(cursor_x, cursor_y);
}

void move_cursor_to(int x, int y)
{
}

static void test_framebuffer_thread()
{
    // Avoid divide by zero
    if (unlikely(fb.mode.height == 0)) {
        printk("Fatail error initializing framebuffer, screen height is zero\n");
        return;
    }

    // Fixedpoint inverse height and scale up to 128
    uint64_t inv_height = 128 * UINT64_C(0x100000000) / fb.mode.height;

    auto intens_fn = [=](int i) { return (128 + ((i * inv_height) >> 32)); };

    int prev_color = intens_fn(0) * 0x010101;
    int prev_st = 0;
    for (size_t i = 0, e = fb.mode.height; i != e; ++i) {
        // Bias +128 and do fixedpoint 32.32 multiply
        int color = intens_fn(i) * 0x010101;
        if (color != prev_color || (i + 1) == e) {
            fill_rect(0, prev_st, fb.mode.width, i, prev_color);
            prev_st = i;
            prev_color = color;
        }
    }

    fill_rect(0, prev_st, fb.mode.width, fb.mode.height, prev_color);

    fill_rect(20, 30, 600, 700, 0x563412);

    draw_str(20+24, 40+23, u8"There is unicode support: ❤✓☀★☂♞☯,☭",
                         0x777777U, 0x563412);

    static char const * const tests[] = {
        u8"Czech: Písmo podporuje mnoho písmen",
        u8"Danish: Skrifttypen understøtter mange bogstaver",
        u8"Greek: Η γραμματοσειρά υποστηρίζει πολλά γράμματα",
        u8"Spanish: La fuente soporta muchas letras.",
        u8"Finnish: Kirjasin tukee monia kirjaimia",
        u8"French: La police supporte beaucoup de lettres",
        u8"Hindi: फ़ॉन्ट कई अक्षरों का समर्थन करता है",
        u8"Armenian: Տառատեսակն ապահովում է բազմաթիվ տառեր",
        u8"Italian: Il font supporta molte lettere",
        u8"Hebrew: הגופן תומך במכתבים רבים",
        u8"Japanese: フォントは多くの文字をサポートしています",
        u8"Korean: 글꼴은 많은 문자를 지원합니다.",
        u8"Latin: The font supports many letters",
        u8"Lithuanian: Šriftas palaiko daug laiškų",
        u8"Dutch: Het lettertype ondersteunt veel letters",
        u8"Polish: Czcionka obsługuje wiele liter",
        u8"Portuguese: A fonte suporta muitas letras",
        u8"Romanian: Fontul acceptă multe litere",
        u8"Russian: Шрифт поддерживает много букв",
        u8"Swedish: Teckensnittet stöder många bokstäver",
        u8"Thai: ตัวอักษรรองรับตัวอักษรหลายตัว",
        u8"Vietnamese: Phông chữ hỗ trợ nhiều chữ cái",
        u8"Chinese: 该字体支持许多字母",
        u8"Chinese (Simplified): 该字体支持许多字母",
        u8"Chinese (Traditional): 該字體支持許多字母"
    };

    int y = 0;
    int x = 0;
    int xdir = 1;
    int ydir = 1;
    for (int i = 0; i < 1; ++i) {
        for (size_t i = 0; i < countof(tests); ++i) {
            draw_str(x + 24 + 40, y + 40 + 33 + 14 + i*20,
                     tests[i], 0x777777U, 0x563412);
        }

        x += xdir;
        if (x > 100 || x < 1) {
            xdir = -xdir;
            x += xdir;
            y += ydir;
            if (y > 200 || y < 1) {
                ydir = -ydir;
                y += ydir;
            }
        }
    }

    scroll_up(16, 0xFF0000);
}

bool framebuffer_console_t::init()
{
    static_init();

    fb_init();

    width = fb.mode.width / 9;
    height = fb.mode.height / CHARHEIGHT;

    ofs_x = (fb.mode.width - width * 9) >> 1;
    ofs_y = (fb.mode.height - height * CHARHEIGHT) >> 1;

    test_framebuffer_thread();

    return true;
}

void framebuffer_console_t::cleanup()
{

}

int framebuffer_console_t::set_dimensions(int width _unused,
                                          int height _unused)
{
    return -int(errno_t::ENOSYS);
}

void framebuffer_console_t::get_dimensions(int *width, int *height)
{
    if (width)
        *width = this->width;
    if (height)
        *height = this->height;
}

void framebuffer_console_t::goto_xy(int x _unused, int y _unused)
{
    cursor_x = ext::min(ext::max(x, 0), width - 1);
    cursor_y = ext::min(ext::max(y, 0), height - 1);
}

int framebuffer_console_t::get_x()
{
    return cursor_x;
}

int framebuffer_console_t::get_y()
{
    return cursor_y;
}

void framebuffer_console_t::fg_set(int color _unused)
{
    fg_color = color;
}

int framebuffer_console_t::fg_get()
{
    return fg_color;
}

void framebuffer_console_t::bg_set(int color _unused)
{
    bg_color = color;
}

int framebuffer_console_t::bg_get()
{
    return bg_color;
}

int framebuffer_console_t::cursor_toggle(int show _unused)
{
    bool was_shown = cursor_on;
    cursor_on = true;
    return was_shown;
}

int framebuffer_console_t::cursor_is_shown()
{
    return cursor_on;
}


//// negative x move the screen content left
//// negative y move the screen content up
void framebuffer_console_t::scroll_screen(int x, int y)
{
    int row_size = width;
    int row_count;
    int row_step;
    int clear_start_row;
    int clear_end_row;
    int clear_start_col;
    int clear_end_col;
    void *src;
    void *dst;

    // Extreme move distance clears screen
    if ((x < 0 && -x >= width) ||
            (x > 0 && x >= width) ||
            (y < 0 && -y >= height) ||
            (y > 0 && y >= height)) {
        clear_screen();

        // Cursor tries to move with content

        cursor_x += x;
        cursor_y += y;

        cap_position(&cursor_x, &cursor_y);
        move_cursor_if_on();
        return;
    }

    if (y <= 0) {
        // Up
        src = (char*)shadow - (y * width) * fb.mode.byte_pp;
        dst = (char*)shadow;
        row_count = height + y;
        // Loop from top to bottom
        row_step = width;
        // Clear the bottom
        clear_end_row = height;
        clear_start_row = clear_end_row + y;
    } else {
        // Down
        dst = shadow + ((height - 1) * width);
        src = shadow + ((height - y - 1) * width);
        row_count = height - y;
        // Loop from bottom to top
        row_step = -width;
        // Clear the top
        clear_start_row = 0;
        clear_end_row = y;
    }

    if (x <= 0) {
        // Left
        row_size += x;
        src -= x;
        // Clear right side
        clear_start_col = width + x;
        clear_end_col = width;
    } else {
        // Right
        row_size -= x;
        src += x;
        // Clear left side
        clear_start_col = 0;
        clear_end_col = x;
    }

    while (row_count--) {
        memmove(dst, src, row_size * fb.mode.byte_pp);
        dst += row_step;
        src += row_step;
    }

    // Clear top/bottom
    if (clear_start_row != clear_end_row) {
        fill_region(
                    0,
                    clear_start_row,
                    width,
                    clear_end_row,
                    ' ');
    }

    // Clear left/right
    if (clear_start_col != clear_end_col) {
        fill_region(
                    clear_start_col,
                    clear_start_row ? clear_start_row : clear_end_row,
                    clear_end_col,
                    clear_start_row ? height : clear_start_row,
                    ' ');
    }

    int mouse_was_shown = mouse_toggle(0);
//    memcpy(video_mem, shadow,
//           width * height * sizeof(*shadow));
    mouse_toggle(mouse_was_shown);
}

void framebuffer_console_t::cap_position(int *px, int *py)
{
    if (*px < 0)
        *px = 0;
    else if (*px >= width)
        *px = width - 1;

    if (*py < 0)
        *py = 0;
    else if (*py >= height)
        *py = height - 1;
}

void framebuffer_console_t::advance_cursor(int distance)
{
    cursor_x += distance;
    if (cursor_x >= width) {
        cursor_x = 0;
        if (++cursor_y >= height) {
            cursor_y = height - 1;
            scroll_screen(0, -1);
        }
    }
}

void framebuffer_console_t::putc(int character _unused)
{
//    int advance;
//    switch (character) {
//    case '\n':
//        advance_cursor(width - cursor_x);
//        break;

//    case '\r':
//        advance_cursor(-cursor_x);
//        break;

//    case '\t':
//        advance = ((cursor_x + 8) & -8) - cursor_x;
//        advance_cursor(advance);
//        break;

//    case '\b':
//        if (cursor_x > 0)
//            advance_cursor(-1);
//        break;

//    default:
//        write_char_at(cursor_x, cursor_y, ch, attrib);
//        advance_cursor(1);
//        break;
//    }
}

void framebuffer_console_t::putc_xy(int x _unused, int y _unused,
                                    int character _unused)
{
    auto glyph = framebuffer_console_t::glyph(character);
    fb_draw_char(x * 9, y * 16, glyph, fg_color, bg_color);

}

int framebuffer_console_t::print(char const *s _unused)
{
    return 0;
}

int framebuffer_console_t::write(char const *s _unused, intptr_t len _unused)
{
    for (intptr_t i = 0; i < len; ++i) {
        switch (*s) {
        default:
            putc_xy(cursor_x, cursor_y, *s);
            advance_cursor(1);
            continue;

        case '\n':
            next_line();
            break;

        }
    }
    return 0;
}

int framebuffer_console_t::print_xy(int x, int y, char const *s _unused)
{
    return 0;
}

int framebuffer_console_t::draw(char const *s _unused)
{
    return 0;
}

int framebuffer_console_t::draw_xy(int x _unused, int y _unused,
                                   char const *s _unused, int attrib _unused)
{
    return 0;
}

void framebuffer_console_t::fill(int sx _unused, int sy _unused,
                                 int ex _unused, int ey _unused,
                                 int character _unused)
{
    for (int x = sx; x < ex; ++x) {
        for (int y = sy; y < ey; ++y) {
            putc_xy(x, y, character);
        }
    }
}

void framebuffer_console_t::clear()
{
    //uint32_t bg = 0x562312;
    //fill_rect(0, 0, fb.mode.width, fb.mode.height, bg);
}

void framebuffer_console_t::scroll(int sx _unused, int sy _unused,
                                   int ex _unused, int ey _unused,
                                   int xd _unused, int yd _unused,
                                   int clear _unused)
{

}

int framebuffer_console_t::mouse_supported()
{
    return 0;
}

int framebuffer_console_t::mouse_is_shown()
{
    return 0;
}

int framebuffer_console_t::mouse_get_x()
{
    return 0;
}

int framebuffer_console_t::mouse_get_y()
{
    return 0;
}

void framebuffer_console_t::mouse_goto_xy(int x, int y)
{
}

void framebuffer_console_t::mouse_add_xy(int x _unused, int y _unused)
{
}

int framebuffer_console_t::mouse_toggle(int show _unused)
{
    return 0;
}

int framebuffer_console_factory_t::detect(text_dev_base_t ***ptrs)
{
    static text_dev_base_t *devs[countof(framebuffer_console_t::instances)];

    if (unlikely(framebuffer_console_t::instance_count >=
                 countof(framebuffer_console_t::instances))) {
        printdbg("Too many VGA devices!\n");
        return 0;
    }

    framebuffer_console_t *self = framebuffer_console_t::instances +
            framebuffer_console_t::instance_count;

    *ptrs = devs + framebuffer_console_t::instance_count++;
    **ptrs = self;

    if (!self->init())
        return 0;

    return 1;
}

size_t glyph_index(size_t codepoint)
{
    if (likely(replacement && codepoint >= ascii_min && codepoint <= ascii_max))
        return ascii_lookup[codepoint - ascii_min];

    size_t st = 0;
    size_t en = glyph_count;
    size_t md;

    while (st < en) {
        md = ((en - st) >> 1) + st;
        if (glyph_codepoints[md] < codepoint)
            st = md + 1;
        else
            en = md;
    }

    if (unlikely(st >= glyph_count || glyph_codepoints[st] != codepoint))
        return size_t(-1);

    return st;
}

bitmap_glyph_t const *get_glyph(size_t codepoint)
{
    size_t i = glyph_index(codepoint);

    return i != size_t(-1)
            ? &glyphs[i]
            : replacement
              ? &glyphs[replacement]
              : &glyphs[ascii_lookup[' ' - ascii_min]];
}


void fb_change_backing(const vbe_selected_mode_t &mode)
{
    assert(mode.framebuffer_addr != 0);
    fb.backing = (uint8_t*)mode.framebuffer_addr;
    fb.video_mem = (uint8_t*)mode.framebuffer_addr;

    test_framebuffer_thread();
}

surface_t surface_t::map_noclip(rect_t region)
{
    int rw = region.en.x - region.st.x;
    int rh = region.en.y - region.st.y;

    int ap = (w - rw) * byte_pp;

    surface_t result;

    result.x = x + region.st.x;
    result.y = y + region.st.y;
    result.w = region.en.x - region.st.x;
    result.h = region.en.y - region.st.y;
    result.pitch = pitch + ap;
    result.backing = backing;
    result.bpp = bpp;
    result.byte_pp = byte_pp;
    result.pix_fmt = pix_fmt;

    return result;
}

surface_t surface_t::map(rect_t region)
{
    region.st.x = ext::max(region.st.x, 0);
    region.st.y = ext::max(region.st.y, 0);
    region.en.x = ext::max(region.en.x, 0);
    region.en.y = ext::max(region.en.y, 0);

    region.st.x = ext::min(region.st.x, w);
    region.en.x = ext::min(region.en.x, w);
    region.st.y = ext::min(region.st.y, h);
    region.en.y = ext::min(region.en.y, h);

    return map_noclip(region);
}
#endif

EXPORT void fb_change_backing(vbe_selected_mode_t const& mode)
{
//    assert(mode.framebuffer_addr != 0);
//    fb.backing = (uint8_t*)mode.framebuffer_addr;
//    fb.video_mem = (uint8_t*)mode.framebuffer_addr;
}
