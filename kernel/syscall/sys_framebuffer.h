#pragma once
#include "types.h"

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

struct fb_info_t {
    // Framebuffer
    void *vmem;

    // Distance from one scanline to the next
    size_t pitch;

    // Width and height
    int32_t w, h;

    // Position
    int32_t x, y;

    // Size of framebuffer in bytes
    size_t vmem_size;

    pix_fmt_t fmt;
    size_t pixel_sz;

    char reserved[24];
};

int sys_framebuffer_enum(size_t index, size_t count, fb_info_t *result_ptr);
int sys_framebuffer_map(size_t index, size_t count);
