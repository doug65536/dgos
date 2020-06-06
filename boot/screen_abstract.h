#pragma once

#include "types.h"

// ╔╗║╚╝═█
// 01234567
enum boxchar_index_t {
    // top left
    TL,

    // top right
    TR,

    // vertical
    V,

    // vertical single
    VS,

    // bottom left
    BL,

    // bottom right
    BR,

    // horizontal
    H,

    // horizontal single
    HS,

    // Solid
    S,

    // space
    X
};

extern tchar const boxchars[];
