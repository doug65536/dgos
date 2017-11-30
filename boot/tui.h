#pragma once
#include "types.h"

enum struct tui_menu_ent_type_t {
    command,
    checkbox,
    separator
};

struct tui_menu_ent_t {
    char const *label;

    union specific_t {
        void (*command_fn)();
        bool checkbox_checked;
    } u;

    tui_menu_ent_type_t type;
};

struct tui_menu_t {
    tui_menu_ent_t const *entries;
    size_t count;
};
