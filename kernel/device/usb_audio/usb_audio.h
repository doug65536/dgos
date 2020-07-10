#pragma once
#include "types.h"

class audio_pin_t {
    enum struct dir_t {
        in,
        out
    };

    enum struct fmt_t {
        le16,
    };

    uint32_t samp_fmt;
    uint32_t channels;
    uint32_t samp_rate;
};
