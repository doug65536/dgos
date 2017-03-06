#pragma once
#include "types.h"

void fb_init(void);

void fb_update_vidmem(int left, int top, int right, int bottom);

void fb_copy_to(int scr_x, int scr_y, int img_stride,
                int img_w, int img_h,
                uint32_t const *pixels);

void fb_update_dirty(void);
