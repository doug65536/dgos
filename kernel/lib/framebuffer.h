#pragma once
#include "types.h"
#include "vesainfo.h"

#include "vector.h"

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

struct vec2_t {
    int32_t x;
    int32_t y;

    vec2_t() : x(0), y(0) {}
    vec2_t(int32_t x, int32_t y) : x(x), y(y) {}
    vec2_t(vec2_t const&) = default;

    vec2_t operator+(vec2_t const& rhs) const
    {
        return { x + rhs.x, y + rhs.y };
    }

    vec2_t operator-(vec2_t const& rhs) const
    {
        return { x - rhs.x, y - rhs.y };
    }
};

struct rect_t {
    vec2_t st;
    vec2_t en;

    rect_t() : st{}, en{} {}
    rect_t(vec2_t const& st, vec2_t const& en) : st(st), en(en) {}
};

struct backing_t {
    void *pixels;
    vec2_t size;
};

enum struct window_style_t {
    none,
    tool
};

struct win_t {
    win_t *parent;
    std::vector<std::unique_ptr<win_t>> children;

    // The current scroll position
    vec2_t scroll_pos;

    // Position and size of window relative to its parent
    rect_t rect;

    // The offscreen render buffer for this window's content
    backing_t backing;
};

/// Windows can be positioned, sized, and overlapped arbitrarily
/// Each window has a scrollable child area
/// Every window except the root has a parent and each window
/// has a list of child windows.
/// All backings are repeated both vertically and horizontally
/// This cannot be seen unless a vertical and/or horizontal offset
/// is applied to the window, in which case, the window is circular
/// both vertically and horizontally and can efficiently implement
/// a scrollback.
