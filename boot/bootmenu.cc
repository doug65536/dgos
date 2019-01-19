#include "bootmenu.h"
#include "bootloader.h"
#include "vesa.h"
#include "paging.h"

static tui_str_t tui_ena_dis[] = {
    TSTR "disabled",
    TSTR "enabled"
};

static tui_menu_item_t tui_menu[] = {
    {
        TSTR "kernel debugger",
        tui_ena_dis,
        0
    },
    {
        TSTR "serial debug output",
        tui_ena_dis,
        0
    },
    {
        TSTR "display resolution",
        {},
        0
    }
};

static tui_list_t<tui_menu_item_t> boot_menu_items(tui_menu);

static tui_menu_renderer_t boot_menu(&boot_menu_items);

template<typename T>
struct remove_reference
{
    using type = T;
};

template<typename T>
struct remove_reference<T&>
{
    using type = T;
};

template<typename T, typename OutIt,
         typename OutT = typename remove_reference<decltype(**(OutIt*)1)>::type>
OutIt to_str(T n, OutIt out, size_t out_sz, OutT pad = OutT{})
{
    OutT buf[24];

    // We fill from right to left so "end" is at the beginning
    auto out_en = buf;

    // We start off the end of the range,
    // but we predecrement before each store
    auto out_st = buf + (sizeof(buf)/sizeof(*buf));

    auto buf_out = out_st;

    do {
        *--buf_out = '0' + (n % 10);
        n /= 10;
    } while (out != out_en && n);

    size_t len = out_st - buf_out;

    size_t padding = 0;
    if (len < out_sz && pad)
        padding = out_sz - len;

    for (size_t i = 0; i < padding; ++i)
        *out++ = pad;

    do {
        *out++ = *buf_out++;
    } while (buf_out != out_st);

    return out;
}

void boot_menu_show(kernel_params_t &params)
{
    static vbe_mode_list_t const& vbe_modes =
            vbe_enumerate_modes();

    // format is %dx%d-%d:%d:%d:%d-%dbpp
    //            ^  ^  ^  ^  ^  ^  ^
    //            |  |  |  |  |  |  |
    //            |  |  |  |  |  |  +- 5 total bits
    //            |  |  |  |  |  |
    //            |  |  |  |  |  +- 3 reserved bits
    //            |  |  |  |  |
    //            |  |  |  |  +- 3 green bits
    //            |  |  |  |
    //            |  |  |  +- 3 blue bits
    //            |  |  |
    //            |  |  +- 3 red bits
    //            |  |
    //            |  +- 5 vert res
    //            |
    //            +- 5 horz res
    //              ---
    //               27+5=32
    //
    // allocate enough for that "mode" times

    tchar *mode_text_buf = new tchar[vbe_modes.count * 32];

    for (size_t i = 0; i < vbe_modes.count; ++i) {
        tchar *res = mode_text_buf + i * 32;
        tchar *end = res + 32 - 1;
        auto& mode = vbe_modes.modes[i];
        *end = 0;
        res = to_str(mode.width, res, end - res);
        if (res < end) *res++ = 'x';\
        res = to_str(mode.height, res, end - res);
        if (res < end) *res++ = '-';
        res = to_str(mode.mask_size_r, res, end - res);
        if (res < end) *res++ = ':';
        res = to_str(mode.mask_size_g, res, end - res);
        if (res < end) *res++ = ':';
        res = to_str(mode.mask_size_b, res, end - res);
        if (res < end) *res++ = ':';
        res = to_str(mode.mask_size_a, res, end - res);
        if (res < end) *res++ = '-';
        res = to_str(mode.bpp, res, end - res);
        if (res < end) *res++ = 'b';
        if (res < end) *res++ = 'p';
        if (res < end) *res++ = 'p';
        if (res < end) *res++ = 0;
    }

    tui_list_t<tui_str_t> mode_list;
    mode_list.count = vbe_modes.count;
    mode_list.items = new tui_str_t[vbe_modes.count];

    for (size_t i = 0; i < vbe_modes.count; ++i) {
        auto str = mode_text_buf + i * 32;
        mode_list.items[i].len = strlen(str);
        mode_list.items[i].str = str;
    }

    tui_menu[2].options = mode_list;

    boot_menu.center();

    boot_menu.interact_timeout(1000);

    auto mode_index = tui_menu[2].index;
    auto const& mode = vbe_modes.modes[mode_index];

    params.vbe_selected_mode = uintptr_t(&mode);
    params.wait_gdb = tui_menu[0].index != 0;
    params.serial_debugout = tui_menu[1].index != 0;
}
