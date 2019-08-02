#pragma once

#include "types.h"

class module_tag_t {
    union flag_union_t {
        struct active {
            bool has_device :1;
            bool has_vendor:1;
            bool has_class:1;
            bool has_subclass:1;
            bool has_progif:1;
        } bits;
        uint16_t raw;
    };
    int16_t device;
    int16_t vendor;

};