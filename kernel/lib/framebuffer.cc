#include "framebuffer.h"
#include "mm.h"
#include "string.h"
#include "likely.h"
#include "assert.h"
#include "math.h"
#include "printk.h"
#include "utility.h"
#include "bootinfo.h"
#include "vesainfo.h"
#include "dev_text.h"

#define CHARHEIGHT 16

struct bitmap_glyph_t {
    uint8_t bits[CHARHEIGHT];
};

// Linked in font
extern bitmap_glyph_t const _binary_u_vga16_raw_start[];
extern bitmap_glyph_t const _binary_u_vga16_raw_end[];

class framebuffer_console_factory_t
        : public text_dev_factory_t
        , public zero_init_t {
public:
    framebuffer_console_factory_t()
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
    }

    static size_t glyph_index(size_t codepoint);
    static const bitmap_glyph_t *glyph(size_t codepoint);

private:
    // text_dev_base_t interface
    TEXT_DEV_IMPL

    friend class framebuffer_console_factory_t;

    static void static_init();

    int cursor_x;
    int cursor_y;
    int cursor_on;

    int width;
    int height;

    int ofs_x;
    int ofs_y;

    int attrib;

    int mouse_x;
    int mouse_y;
    int mouse_on;

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
};

framebuffer_console_t framebuffer_console_t::instances[1];
unsigned framebuffer_console_t::instance_count;
uint8_t framebuffer_console_t::ascii_lookup[1 + ascii_max - ascii_min];
uint16_t framebuffer_console_t::replacement;
size_t framebuffer_console_t::glyph_count;
uint16_t *framebuffer_console_t::glyph_codepoints;

#define USE_NONTEMPORAL 0

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
    uint8_t *back_buf;
    vbe_selected_mode_t mode;
    fb_rect_t dirty;
};

static framebuffer_t fb;

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

    size_t screen_size = fb.mode.width * fb.mode.height * sizeof(uint32_t);

    // Round the back buffer size up to a multiple of the cache size
    screen_size = (screen_size + 63) & -64;

    fb.back_buf = (uint8_t*)fb.mode.framebuffer_addr;
    fb.video_mem = (uint8_t*)fb.mode.framebuffer_addr;

    //madvise(fb.video_mem, screen_size, MADV_WEAKORDER);

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

    uint8_t *out = fb.back_buf +
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

void fb_fill_rect(int sx, int sy, int ex, int ey, uint32_t color)
{

    fb_update_dirty(sx, sy, ex, ey);

    size_t row_ofs = sx * sizeof(uint32_t);
    size_t row_end = ex * sizeof(uint32_t);

    uint8_t *out = fb.back_buf + sy * fb.mode.pitch + row_ofs;
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
    int adx = std::max(dx, -dx);
    int ady = std::max(dy, -dy);

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
            ((uint32_t*)(fb.back_buf + fb.mode.pitch * y))[x] = pixel_color;
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
        memcpy(fb.video_mem + ofs, fb.back_buf + ofs, copy_width);
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

    if (unlikely(ex > fb.mode.width))
        ex = fb.mode.width;

    if (unlikely(ey > fb.mode.height))
        ey = fb.mode.height;

    if (unlikely(sx < 0))
        sx = 0;

    if (unlikely(sy < 0))
        sy = 0;

    if (unlikely(ex <= sx))
        return;

    if (unlikely(ey <= sy))
        return;

    uint32_t val = rgb(color);

    char *dest = (char*)(fb.video_mem +
                         sy * fb.mode.pitch +
                         sx * fb.mode.byte_pp);
    size_t wid = (ex - sx);
    size_t skip = fb.mode.pitch - (wid * fb.mode.byte_pp);

    switch (fb.mode.byte_pp) {
    case 4:
        while (sy < ey) {
            memset32(dest, val, wid);
            dest += skip;
            ++sy;
        }
        break;

    case 3:
        // FIXME: optimize, take advantage of 4 pixels every 3 dwords
        /// r g b r
        /// g b r g
        /// b r g b
        while (sy < ey) {
            for (int x = sx; x < ex; ++x) {
                *dest++ = val & 0xFF;
                *dest++ = (val >> 8) & 0xFF;
                *dest++ = (val >> 16) & 0xFF;
            }

            dest += skip;
            ++sy;
        }
        break;

    case 1:
        while (sy < ey) {
            memset8(dest, val, wid);
            dest += skip;
            ++sy;
        }
        break;

    case 2:
        while (sy < ey) {
            memset16(dest, val, wid);
            dest += skip;
            ++sy;
        }
        break;

    }
}

static void scroll_rect(int sx, int sy, int ex, int ey, int dx, int dy) {

}

static void draw_char(int x, int y, bitmap_glyph_t const * restrict glyph,
                      uint32_t fg, uint32_t bg)
{
    if (unlikely(unsigned(x + 8) > fb.mode.width ||
                 unsigned(y + CHARHEIGHT) > fb.mode.height))
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

    size_t sx = x;
    size_t sy = y;
    size_t ex = x + 8;
    size_t ey = y + CHARHEIGHT;

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

void scroll_up(size_t dist, uint32_t bg)
{
    if (fb.mode.pitch == fb.mode.width * fb.mode.byte_pp) {
        uint8_t *src = fb.video_mem + dist * fb.mode.pitch;
        uint8_t *dst = fb.video_mem;
        memmove(dst, src, fb.mode.pitch *
                (fb.mode.height - dist));
        fill_rect(0, fb.mode.height - dist,
                  fb.mode.width, fb.mode.height, bg);
    } else {
        // Scanline at a time to avoid touching offscreen
//                        for (size_t y = dist, ey = fb.mode.height;
//                             y < ey; ++y) {
//                            memmove(fb.video_mem, fb.video_mem + fb.)
//                        }
    }
}

static void draw_str(int x, int y, char const* str,
                     uint32_t fg, uint32_t bg)
{
    while (char32_t codepoint = utf8_to_ucs4_inplace(str)) {
        auto glyph = framebuffer_console_t::glyph(codepoint);
        draw_char(x, y, glyph, fg, bg);
        x += 9;

        if (unlikely(x + 9 > fb.mode.width)) {
            // Went off the right side
            x = 0;
            if (unlikely(y + CHARHEIGHT > fb.mode.height)) {
                size_t dist = y + CHARHEIGHT - fb.mode.height;

                scroll_up(dist, bg);
                y = fb.mode.height - CHARHEIGHT;
            } else {
                y += CHARHEIGHT;
            }
        }
    }
}

///

void framebuffer_console_t::static_init()
{
    // Detect the number of items from size
    auto en = uintptr_t(_binary_u_vga16_raw_end);
    auto st = uintptr_t(_binary_u_vga16_raw_start);
    auto sz = en - st;
    glyph_count = sz / (sizeof(bitmap_glyph_t) + sizeof(uint16_t));
    glyph_codepoints = (uint16_t*)(_binary_u_vga16_raw_start + glyph_count);

    if (replacement)
        return;

    // Populate ASCII glyph lookup table
    for (size_t i = ascii_min; i <= ascii_max; ++i)
        ascii_lookup[i - ascii_min] = glyph_index(i);

    // Lookup unicode replacement character
    replacement = glyph_index(0xFFFD);
}

bool framebuffer_console_t::init()
{
    static_init();

    fb_init();

    width = fb.mode.width / 9;
    height = fb.mode.height / CHARHEIGHT;

    ofs_x = (fb.mode.width - width * 9) >> 1;
    ofs_y = (fb.mode.height - height * CHARHEIGHT) >> 1;

    // Fixedpoint inverse height and scale up to 128
    uint64_t inv_height = 128 * UINT64_C(0x100000000) / fb.mode.height;
    for (size_t i = 0, e = fb.mode.height; i != e; ++i) {
        // Bias +128 and do fixedpoint 32.32 multiply
        auto intensity = (128 + ((i * inv_height) >> 32));
        auto color = intensity * 0x010101;
        fill_rect(0, i, fb.mode.width, i + 1, color);
    }

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

    for (size_t i = 0; i < countof(tests); ++i)
        draw_str(24 + 40, 40 + 33 + 14 + i*20, tests[i], 0x777777U, 0x563412);


    return true;
}

void framebuffer_console_t::cleanup()
{

}

int framebuffer_console_t::set_dimensions(int width, int height)
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

void framebuffer_console_t::goto_xy(int x, int y)
{

}

int framebuffer_console_t::get_x()
{
    return 0;
}

int framebuffer_console_t::get_y()
{
    return 0;
}

void framebuffer_console_t::fg_set(int color)
{
}

int framebuffer_console_t::fg_get()
{
    return 0;
}

void framebuffer_console_t::bg_set(int color)
{
}

int framebuffer_console_t::bg_get()
{
    return 0;
}

int framebuffer_console_t::cursor_toggle(int show)
{
    return 0;
}

int framebuffer_console_t::cursor_is_shown()
{
    return 0;
}

void framebuffer_console_t::putc(int character)
{
}

void framebuffer_console_t::putc_xy(int x, int y, int character)
{
}

int framebuffer_console_t::print(const char *s)
{
    return 0;
}

int framebuffer_console_t::write(const char *s, intptr_t len)
{
    return 0;
}

int framebuffer_console_t::print_xy(int x, int y, const char *s)
{
    return 0;
}

int framebuffer_console_t::draw(const char *s)
{
    return 0;
}

int framebuffer_console_t::draw_xy(int x, int y, const char *s, int attrib)
{
    return 0;
}

void framebuffer_console_t::fill(int sx, int sy, int ex, int ey, int character)
{
}

void framebuffer_console_t::clear()
{
    //uint32_t bg = 0x562312;
    //fill_rect(0, 0, fb.mode.width, fb.mode.height, bg);
}

void framebuffer_console_t::scroll(int sx, int sy, int ex, int ey,
                                   int xd, int yd, int clear)
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

void framebuffer_console_t::mouse_add_xy(int x, int y)
{
}

int framebuffer_console_t::mouse_toggle(int show)
{
    return 0;
}

int framebuffer_console_factory_t::detect(text_dev_base_t ***ptrs)
{
    static text_dev_base_t *devs[countof(framebuffer_console_t::instances)];

    if (framebuffer_console_t::instance_count >=
            countof(framebuffer_console_t::instances)) {
        printdbg("Too many VGA devices!\n");
        return 0;
    }

    framebuffer_console_t* self = framebuffer_console_t::instances +
            framebuffer_console_t::instance_count;

    *ptrs = devs + framebuffer_console_t::instance_count++;
    **ptrs = self;

    if (!self->init())
        return 0;

    return 1;
}

size_t framebuffer_console_t::glyph_index(size_t codepoint)
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

bitmap_glyph_t const *framebuffer_console_t::glyph(size_t codepoint)
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
    fb.back_buf = (uint8_t*)mode.framebuffer_addr;
    fb.video_mem = (uint8_t*)mode.framebuffer_addr;
}
