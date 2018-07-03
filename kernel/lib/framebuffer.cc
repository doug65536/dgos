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

#define USE_NONTEMPORAL 1

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

    fb.back_buf = (uint8_t*)mmap(nullptr, screen_size, PROT_READ | PROT_WRITE,
                                 0, -1, 0);
    fb.video_mem = (uint8_t*)(uintptr_t)fb.mode.framebuffer_addr;

    madvise(fb.video_mem, screen_size, MADV_WEAKORDER);

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

//#ifdef __x86_64__
//static void fb_blend_pixel(uint8_t *pixel, __f32_vec4 fcolor, float alpha)
//{
//    //printdbg("alpha=%6.3f\n", (double)alpha);
//    float ooa = 1.0f - alpha;
//    __f32_vec4 tmp1, tmp2;
//    __asm__ __volatile__ (
//        "shufps $0,%[alpha],%[alpha]\n\t"
//        "shufps $0,%[ooa],%[ooa]\n\t"
//        "mulps %[ooa],%[fcolor]\n\t"
//        // Load 32 bit pixel
//        "movd (%[pixel]),%[tmp1]\n\t"
//        // Unpack to 16 bit vector
//        "punpcklbw %[zero],%[tmp1]\n\t"
//        // Foreground color
//        // Unpack to 32 bit vector
//        "punpcklwd %[zero],%[tmp1]\n\t"
//        // Convert pixel RGBA to floating point
//        "cvtdq2ps %[tmp1],%[tmp1]\n\t"
//        // Multiply pixel color by 1.0f - alpha
//        "mulps %[alpha],%[tmp1]\n\t"
//        // Add prescaled foreground color to result
//        "addps %[fcolor],%[tmp1]\n\t"
//        // Clamp
//        "maxps %[zero],%[tmp1]\n\t"
//        // Convert back to integer
//        "cvttps2dq %[tmp1],%[tmp1]\n\t"
//        // Pack to 16 bit vector
//        "packssdw %[zero],%[tmp1]\n\t"
//        // Pack to 8 bit vector
//        "packuswb %[zero],%[tmp1]\n\t"
//        // Store result pixel
//        "movd %[tmp1],(%[pixel])\n\t"
//        : [tmp1] "=&x" (tmp1), "=&x" (tmp2)
//        , [alpha] "+x" (alpha)
//        , [ooa] "+x" (ooa)
//        , [fcolor] "+x" (fcolor)
//        : [pixel] "r" (pixel)
//        , [zero] "x" (0)
//        : "memory"
//    );
//}
//#else
//static void fb_blend_pixel(uint8_t *pixel, __f32_vec4 fcolor, float alpha)
//{
//    float ooa = 1.0f - alpha;
//    pixel[0] = fcolor[0] * alpha + apixel[0] * ooa;
//    pixel[1] = fcolor[1] * alpha + apixel[1] * ooa;
//    pixel[2] = fcolor[2] * alpha + apixel[2] * ooa;
//    pixel[3] = fcolor[3] * alpha + apixel[3] * ooa;
//}
//#endif

#if 1
template<typename F>
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color, F setpixel)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = max(dx, -dx);
    int ady = max(dy, -dy);

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
