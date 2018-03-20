#include "bootmenu.h"
#include "bootloader.h"

static tui_str_t tui_ena_dis[] = {
    "disabled",
    "enabled"
};

static tui_menu_item_t tui_menu[] = {
    {
        "kernel debugger",
        tui_ena_dis,
        0
    },
    {
        "serial debug output",
        tui_ena_dis,
        0
    }
};

static tui_list_t<tui_menu_item_t> boot_menu_items(tui_menu);

static tui_menu_renderer_t boot_menu(&boot_menu_items);

void boot_menu_show(kernel_params_t &params)
{
    boot_menu.center();
    boot_menu.interact_timeout(3000);
    params.wait_gdb = tui_menu[0].index != 0;
    params.serial_debugout = tui_menu[1].index != 0;
}
