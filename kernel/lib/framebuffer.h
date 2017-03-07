#pragma once
#include "types.h"

void fb_init(void);

void fb_copy_to(int scr_x, int scr_y, int img_stride,
                int img_w, int img_h,
                uint32_t const *pixels);

void fb_fill_rect(int sx, int sy,
                  int ex, int ey, uint32_t color);

void fb_update(void);
