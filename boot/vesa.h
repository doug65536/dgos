#pragma once
#include "types.h"
#include "vesainfo.h"

// Move only heap array with count
struct vbe_mode_list_t {
    vbe_selected_mode_t *modes;
    size_t count;

    vbe_mode_list_t()
        : modes{}
        , count{}
    {
    }

    explicit vbe_mode_list_t(size_t count)
        : modes{new vbe_selected_mode_t[count]}
        , count(count)
    {
    }

    vbe_mode_list_t(vbe_mode_list_t&& rhs)
        : modes{rhs.modes}
        , count{rhs.count}
    {
        rhs.modes = nullptr;
        rhs.count = 0;
    }

    vbe_mode_list_t& operator=(vbe_mode_list_t) = delete;

    ~vbe_mode_list_t()
    {
        delete[] modes;
        modes = nullptr;
        count = 0;
    }
};

struct vbe_mode_key_t {
    uint16_t h_res;
    uint16_t v_res;
    uint8_t r_bits;
    uint8_t g_bits;
    uint8_t b_bits;
    uint8_t a_bits;
};

vbe_mode_list_t const& vbe_enumerate_modes();

bool vbe_set_mode(vbe_selected_mode_t &mode);

vbe_selected_mode_t *vbe_select_mode(
        uint32_t width, uint32_t height, bool verbose);

void aspect_ratio(uint16_t *n, uint16_t *d, uint16_t w, uint16_t h);
