// If being included from the kernel, avoid our types.h
#ifndef __x86_64__
#include "types.h"
#endif

struct vbe_selected_mode_t {
    uint32_t framebuffer_addr;
    uint32_t framebuffer_bytes;

    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t aspect_n;
    uint16_t aspect_d;

    uint8_t mask_size_r;
    uint8_t mask_size_g;
    uint8_t mask_size_b;
    uint8_t mask_size_a;
    uint8_t mask_pos_r;
    uint8_t mask_pos_g;
    uint8_t mask_pos_b;
    uint8_t mask_pos_a;

    char reserved[6];
};
