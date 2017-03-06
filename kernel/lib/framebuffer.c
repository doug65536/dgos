#include "framebuffer.h"
#include "mm.h"
#include "string.h"
#include "likely.h"
#include "assert.h"

#include "../boot/vesainfo.h"


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
    fb.video_mem = (void*)(uintptr_t)fb.mode.framebuffer_addr;

    madvise(fb.video_mem, screen_size, MADV_WEAKORDER);

    fb_reset_dirty();
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

    // Update dirty rectangle
    fb.dirty.en.x = (fb.dirty.en.x < scr_ex ? scr_ex : fb.dirty.en.x);
    fb.dirty.en.y = (fb.dirty.en.y < scr_ey ? scr_ey : fb.dirty.en.y);
    fb.dirty.st.x = (fb.dirty.st.x > scr_x ? scr_x : fb.dirty.st.x);
    fb.dirty.st.y = (fb.dirty.st.y > scr_y ? scr_y : fb.dirty.st.y);

    uint8_t *out = fb.back_buf +
            (scr_y * fb.mode.pitch + scr_x * sizeof(uint32_t));

    for (int y = scr_y; y < scr_ey; ++y, out += fb.mode.pitch) {
        memcpy32_nt(out, pixels, img_w * sizeof(uint32_t));
        pixels += img_pitch;
    }
}

void fb_update_vidmem(int left, int top, int right, int bottom)
{
    assert(left >= 0);
    assert(top >= 0);
    assert(right <= fb.mode.width);
    assert(bottom <= fb.mode.height);

    size_t width = (right - left) * sizeof(uint32_t);
    size_t row_ofs = top * fb.mode.pitch + left * sizeof(uint32_t);

    for (size_t y = (unsigned)top; y < (unsigned)bottom;
         ++y, row_ofs += fb.mode.pitch) {
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
        memcpy512_nt(fb.video_mem + ofs, fb.back_buf + ofs, copy_width);
    }
    memcpy_nt_fence();
}

void fb_update_dirty(void)
{
    fb_update_vidmem(fb.dirty.st.x, fb.dirty.st.y,
                     fb.dirty.en.x, fb.dirty.en.y);
    fb_reset_dirty();
}
