#include "bootmenu.h"
#include "bootloader.h"
#include "vesa.h"
#include "paging.h"
#include "malloc.h"
#include "qemu.h"
#include "screen.h"
#include "utf.h"

static tui_str_t tui_timeout[] = {
    TSTR "1.5",
    TSTR "5",
    TSTR "10",
    TSTR "20",
    TSTR "never"
};

static tui_str_t tui_com_port[] = {
    TSTR "disabled",
    TSTR "com1",
    TSTR "com2",
    TSTR "com3",
    TSTR "com4"
};

static tui_str_t tui_baud_rates[] = {
    TSTR "115200 (max speed)",
    TSTR "57600",
    TSTR "38400",
    TSTR "19200",
    TSTR "14400",
    TSTR "9600",
    TSTR "4800",
    TSTR "2400",
    TSTR "1200",
    TSTR "600",
    TSTR "300 (very slow)"
};

enum tui_menu_item_index_t {
    menu_timeout,
    kernel_debugger,
    testrun_port,
    serial_output,
    serial_baud,
    display_resolution,
    e9_enable,
    smp_enable,
    acpi_enable,
    mps_enable,
    msi_enable,
    msix_enable,
    command_line
};

//opt/com.doug16k.dgos.bootmenu.timeout
//opt/com.doug16k.dgos.bootmenu.kd_port
//opt/com.doug16k.dgos.bootmenu.dbgout_port
//opt/com.doug16k.dgos.bootmenu.dbgout_rate
//opt/com.doug16k.dgos.bootmenu.display_res
//opt/com.doug16k.dgos.bootmenu.e9_out
//opt/com.doug16k.dgos.bootmenu.smp
//opt/com.doug16k.dgos.bootmenu.acpi
//opt/com.doug16k.dgos.bootmenu.mps
//opt/com.doug16k.dgos.bootmenu.msi
//opt/com.doug16k.dgos.bootmenu.msix
//opt/com.doug16k.dgos.bootmenu.cmdline

static tui_menu_item_t tui_menu[] = {
    { TSTR "timeout", TSTR "menu timeout (sec)", tui_timeout, 0 },
    { TSTR "kd_port", TSTR "kernel debugger", tui_com_port, 0 },
    { TSTR "testrun_port", TSTR "Test run port", tui_com_port, 1 },
    { TSTR "dbgout_port", TSTR "serial debug output", tui_com_port, 1 },
    { TSTR "dbgout_rate", TSTR "serial baud rate", tui_baud_rates, 0 },
    { TSTR "display_res", TSTR "display resolution", {}, 0 },
    { TSTR "e9_out", TSTR "port 0xe9 debug output", tui_dis_ena, 0 },
    { TSTR "smp", TSTR "SMP", tui_dis_ena, 1 },
    { TSTR "acpi", TSTR "ACPI", tui_dis_ena, 1 },
    { TSTR "mps", TSTR "MPS", tui_dis_ena, 1 },
    { TSTR "msi", TSTR "MSI", tui_dis_ena, 1 },
    { TSTR "msix", TSTR "MSI-X", tui_dis_ena, 1 },
    { TSTR "cmdline", TSTR "command line", 511 }
};

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
    auto out_st = buf + (sizeof(buf) / sizeof(*buf));

    auto buf_out = out_st;

    do {
        *--buf_out = '0' + (n % 10);
        n /= 10;
    } while (out != out_en && n);

    size_t len = size_t(out_st - buf_out);

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

bool mode_is_bgrx32(vbe_selected_mode_t const& mode)
{
    return (mode.byte_pp) == 4 ||
            (mode.mask_pos_r) == 16 ||
            (mode.mask_pos_g) == 8 ||
            (mode.mask_pos_b) == 0 ||
            (mode.mask_size_r) == 8 ||
            (mode.mask_size_g) == 8 ||
            (mode.mask_size_b) == 8;
}

void apply_bootmenu_fw_cfg(tui_list_t<tui_menu_item_t>& boot_menu_items)
{
    char name[48];
    char value[16];
    constexpr tui_str_t prefix{TSTR "opt/com.doug16k.dgos.bootmenu."};
    size_t prefix_len = tchar_to_utf8(name, sizeof(name),
                                      prefix.str, prefix.len);
    size_t prefix_remain = sizeof(name) - prefix_len;

    PRINT("applying bootmenu fw_cfg");

    for (size_t i = 0; i < boot_menu_items.count; ++i) {
        tui_menu_item_t& menu_item = boot_menu_items.items[i];
        tui_str_t& menu_item_name = menu_item.name;

        tchar_to_utf8(name + prefix_len, prefix_remain,
                      menu_item_name.str, menu_item_name.len);

        uint32_t file_sz = 0;
        int selector = qemu_selector_by_name(name, &file_sz);

        if (likely(selector < 0)) {
            PRINT("selector not found for %s", name);
            continue;
        }

        size_t read_size = file_sz < sizeof(value) ? file_sz : sizeof(value);
        if (unlikely(!qemu_fw_cfg(value, read_size, file_sz, selector))) {
            PRINT("failed to read selector!");
            continue;
        }

        assert(file_sz < sizeof(value) - 1);
        value[file_sz] = 0;

        bool found = false;
        for (size_t index = 0; index < menu_item.options.count; ++index) {
            tui_str_t const& option = menu_item.options.items[index];
            if (!tchar_strcmp_utf8(option.str, value)) {
                menu_item.index = index;
                found = true;
                break;
            }
        }

        if (unlikely(!found)) {
            PRINT("Failed to find value %s for option %s",
                  value, menu_item.name.str);
        }
    }
}

void boot_menu_show(kernel_params_t &params)
{
    tui_list_t<tui_menu_item_t> boot_menu_items(tui_menu);

    apply_bootmenu_fw_cfg(boot_menu_items);

    tui_menu_renderer_t boot_menu(boot_menu_items);

    vbe_mode_list_t const& vbe_modes =
            vbe_enumerate_modes();

    // format is %dx%d-%d:%d:%d:%d-%dbpp (fastest)
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
    //               27+5+9=41
    //
    // allocate enough for that "mode" times

    tchar *mode_text_buf = new (ext::nothrow) tchar[vbe_modes.count * 64];

    if (unlikely(!mode_text_buf))
        PANIC_OOM();

    for (size_t i = 0; i < vbe_modes.count; ++i) {
        tchar *res = mode_text_buf + i * 64;
        tchar *end = res + 64 - 1;
        auto& mode = vbe_modes.modes[i];
        *end = 0;

        res = to_str(mode.width, res, size_t(end - res));

        if (res < end) *res++ = 'x';\

        res = to_str(mode.height, res, size_t(end - res));

        if (res < end) *res++ = '-';

        res = to_str(mode.mask_size_r, res, size_t(end - res));

        if (res < end) *res++ = ':';

        res = to_str(mode.mask_size_g, res, size_t(end - res));

        if (res < end) *res++ = ':';

        res = to_str(mode.mask_size_b, res, size_t(end - res));

        if (res < end) *res++ = ':';

        res = to_str(mode.mask_size_a, res, size_t(end - res));

        if (res < end) *res++ = '-';

        res = to_str(mode.bpp, res, size_t(end - res));

        if (res + 5 < end) {
            strcpy(res, TSTR "bpp ");
            res += 4;
        }

        if (mode_is_bgrx32(mode) &&
                res + 6 < end) {
            strcpy(res, TSTR "(fastest)");
            res += 6;
        } else if (mode.byte_pp == 4 &&
                mode.mask_pos_r == 0 &&
                mode.mask_pos_g == 8 &&
                mode.mask_pos_b == 16 &&
                mode.mask_size_r == 8 &&
                mode.mask_size_g == 8 &&
                mode.mask_size_b == 8 &&
                res + 9 < end) {
            strcpy(res, TSTR "(fast)");
            res += 9;
        } else if (res + 6 < end) {
            strcpy(res, TSTR "(slow)");
            res += 6;
        }

        if (res < end) *res++ = 0;
    }

    tui_list_t<tui_str_t> mode_list;
    mode_list.count = vbe_modes.count;
    mode_list.items = new (ext::nothrow) tui_str_t[vbe_modes.count];

    for (size_t i = 0; i < vbe_modes.count; ++i) {
        auto str = mode_text_buf + i * 64;
        mode_list.items[i].len = strlen(str);
        mode_list.items[i].str = str;
    }

    tui_menu[display_resolution].options = mode_list;

    boot_menu.center(5);

    boot_menu.interact_timeout(1500);

    auto mode_index = tui_menu[display_resolution].index;
    auto const* mode = &vbe_modes.modes[mode_index];

    vbe_selected_mode_t *aligned_mode = (vbe_selected_mode_t*)
            malloc(sizeof(vbe_selected_mode_t));
    memcpy(aligned_mode, mode, sizeof(*aligned_mode));

    params.vbe_selected_mode = uintptr_t(aligned_mode);
    params.gdb_port = tui_menu[kernel_debugger].index;
    params.testrun_port = tui_menu[testrun_port].index;
    params.serial_debugout = tui_menu[serial_output].index;
    params.serial_baud = tui_menu[serial_baud].index;
    params.smp_enable = tui_menu[smp_enable].index;
    params.acpi_enable = tui_menu[acpi_enable].index;
    params.mps_enable = tui_menu[mps_enable].index;
    params.msi_enable = tui_menu[msi_enable].index;
    params.msix_enable = tui_menu[msix_enable].index;
    params.e9_enable = tui_menu[e9_enable].index;
    params.command_line = utf8_from_tchar(
                tui_menu[command_line].text);
}
