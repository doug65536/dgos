#include "framebuffer.h"
#include "mm.h"
#include "string.h"
#include "likely.h"
#include "assert.h"
#include "math.h"
#include "printk.h"

#include "../boot/vesainfo.h"

#define USE_NONTEMPORAL 1

typedef struct fb_coord_t {
    int x;
    int y;
} fb_coord_t;

typedef struct fb_rect_t {
    fb_coord_t st;
    fb_coord_t en;
} fb_rect_t;

typedef struct framebuffer_t {
    uint8_t *video_mem;
    uint8_t *back_buf;
    vbe_selected_mode_t mode;
    fb_rect_t dirty;
} framebuffer_t;

static framebuffer_t fb;

static inline void fb_reset_dirty(void)
{
    fb.dirty.st.x = fb.mode.width;
    fb.dirty.st.y = fb.mode.height;
    fb.dirty.en.x = 0;
    fb.dirty.en.y = 0;
}

void fb_init(void)
{
    fb.mode = *(vbe_selected_mode_t*)(uintptr_t)
            *(uint32_t*)(uintptr_t)(0x7C00 + 72);

    size_t screen_size = fb.mode.width * fb.mode.height * sizeof(uint32_t);

    // Round the back buffer size up to a multiple of the cache size
    screen_size = (screen_size + 63) & -64;

    fb.back_buf = mmap(0, screen_size, PROT_READ | PROT_WRITE, 0, -1, 0);
    fb.video_mem = (uint8_t*)(uintptr_t)fb.mode.framebuffer_addr;

    madvise(fb.video_mem, screen_size, MADV_WEAKORDER);

    fb_reset_dirty();
}

static inline void fb_update_dirty(int left, int top, int right, int bottom)
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

#ifdef __x86_64__
static void fb_blend_pixel(uint8_t *pixel, __fvec4 fcolor, float alpha)
{
    //printdbg("alpha=%6.3f\n", (double)alpha);
    float ooa = 1.0f - alpha;
    __fvec4 tmp1, tmp2;
    __asm__ __volatile__ (
        "shufps $0,%[alpha],%[alpha]\n\t"
        "shufps $0,%[ooa],%[ooa]\n\t"
        "mulps %[ooa],%[fcolor]\n\t"
        // Load 32 bit pixel
        "movd (%[pixel]),%[tmp1]\n\t"
        // Unpack to 16 bit vector
        "punpcklbw %[zero],%[tmp1]\n\t"
        // Foreground color
        // Unpack to 32 bit vector
        "punpcklwd %[zero],%[tmp1]\n\t"
        // Convert pixel RGBA to floating point
        "cvtdq2ps %[tmp1],%[tmp1]\n\t"
        // Multiply pixel color by 1.0f - alpha
        "mulps %[alpha],%[tmp1]\n\t"
        // Add prescaled foreground color to result
        "addps %[fcolor],%[tmp1]\n\t"
        // Clamp
        "maxps %[zero],%[tmp1]\n\t"
        // Convert back to integer
        "cvttps2dq %[tmp1],%[tmp1]\n\t"
        // Pack to 16 bit vector
        "packssdw %[zero],%[tmp1]\n\t"
        // Pack to 8 bit vector
        "packuswb %[zero],%[tmp1]\n\t"
        // Store result pixel
        "movd %[tmp1],(%[pixel])\n\t"
        : [tmp1] "=&x" (tmp1), "=&x" (tmp2)
        , [alpha] "+x" (alpha)
        , [ooa] "+x" (ooa)
        , [fcolor] "+x" (fcolor)
        : [pixel] "r" (pixel)
        , [zero] "x" (0)
        : "memory"
    );
}
#else
static void fb_blend_pixel(uint8_t *pixel, __fvec4 fcolor, float alpha)
{
    float ooa = 1.0f - alpha;
    pixel[0] = fcolor[0] * alpha + apixel[0] * ooa;
    pixel[1] = fcolor[1] * alpha + apixel[1] * ooa;
    pixel[2] = fcolor[2] * alpha + apixel[2] * ooa;
    pixel[3] = fcolor[3] * alpha + apixel[3] * ooa;
}
#endif

void fb_draw_aa_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    fb_update_dirty(x0, y0, x1, y1);

    __fvec4 fcolor = {
        float(color & 0xFF),
        float((color >> 8) & 0xFF),
        float((color >> 16) & 0xFF),
        float((color >> 24) & 0xFF)
    };
    uint8_t *addr = fb.back_buf + (y0 * fb.mode.pitch + x0 * sizeof(uint32_t));
    int dx = x1 - x0;
    int dy = y1 - y0;

    int du, dv, u, v, uincr, vincr;

    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    if (abs_dx > abs_dy)
    {
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
    int d = (2 * dv) - du;
    int incrS = 2 * dv;
    int incrD = 2 * (dv - du);
    int twovdu = 0;
    float invD = 1.0f / (2.0f * sqrtf(du*du + dv*dv));
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
