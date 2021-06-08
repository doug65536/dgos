#pragma once
#include "types.h"
#include "modeinfo.h"

#include "vector.h"
#include "cxxstring.h"

__BEGIN_DECLS

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

void fb_draw_char(int x, int y, char32_t codepoint, uint32_t fg, uint32_t bg);

__END_DECLS

// 2D int vector
struct vec2_t {
    int32_t x;
    int32_t y;

    vec2_t() : x(0), y(0) {}
    vec2_t(int32_t x, int32_t y) : x(x), y(y) {}
    vec2_t(vec2_t const&) = default;
    vec2_t& operator=(vec2_t const&) = default;

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

    inline bool is_inside(vec2_t const& p) const
    {
        return p.x >= st.x && p.y >= st.y &&
                p.x < en.x && p.y < en.y;
    }
};

// Backing that represents a raw byte array
struct backing_t {
    void *mem;
    size_t byte_size;
};

// Is rectangular
struct rect_backing_t : public backing_t {
    vec2_t size;

    // Clipped blit a rectangle from this backing to this backing or another
    void blt_to(rect_backing_t const& dest,
                vec2_t const& dest_pos, vec2_t const& area,
                vec2_t const& src_pos);

    void fill(vec2_t const& pos, vec2_t const& area, uint32_t color);
};

enum struct window_style_t {
    // No borders or decorations
    none,

    // Small title window
    tool,

    // Full sized application window
    app
};

struct win_t;

struct win_event_t {
    int64_t timestamp;
    vec2_t clientPt;
    vec2_t screenPt;

    int64_t regionId;
    refptr<win_t> target;
    refptr<win_t> related_target;

    // Bitmask of mouse buttons held at the moment this event occurred
    int32_t buttons;
    int32_t button;

    // mousedown and click will have the click count here
    int32_t detail;

    // Whether modifiers were held
    bool held_shift;
    bool held_ctrl;
    bool held_alt;
    bool held_meta;

    // State data
    bool num_locked;
    bool caps_locked;
    bool scroll_locked;
    bool reserved_locked;

    // Event flags
    bool event_bubbles;
    bool event_cancelable;
};

struct win_listener_t : refcounted<win_listener_t> {
    virtual void win_event(win_event_t const& event) = 0;
};

struct win_t : refcounted<win_t> {
    win_t *parent;
    ext::vector<refptr<win_t>> children;

    // The current scroll position
    vec2_t scroll_pos;

    // The maximum scroll offset
    vec2_t scroll_max;

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
