#pragma once
#include "types.h"
#include "vesainfo.h"

void fb_init(void);

void fb_change_backing(vbe_selected_mode_t const& mode);
void fb_copy_to(int scr_x, int scr_y, int img_stride,
                int img_w, int img_h,
                uint32_t const *pixels);

void fb_fill_rect(int sx, int sy,
                  int ex, int ey, uint32_t color);

void fb_draw_aa_line(int x0, int y0, int x1, int y1, uint32_t color);
void fb_clip_aa_line(int x0, int y0, int x1, int y1);


void fb_update(void);
