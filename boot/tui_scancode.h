#pragma once

// Returns scancode in ah, ascii in al
//       esc -> 0x011B
//       '0' -> 0x0B30
//       '1' -> 0x0231
//       'A' -> 0x1E41
//       'a' -> 0x1E61
//     enter -> 0x1C2D
// num enter -> 0xE00D
//       '+' -> 0x0D2B
//     num + -> 0x4E2B
//       '-' -> 0x4A2D
//     num - -> 0x4A2D
//       '/' -> 0x352F
//     num / -> 0xE02F
//       '*' -> 0x092A
//     num * -> 0x372A

enum bioskey_e0_t {
    key_up    = 0x48E0,
    key_down  = 0x50E0,
    key_left  = 0x4BE0,
    key_right = 0x4DE0,
    key_ins   = 0x52E0,
    key_home  = 0x47E0,
    key_pgup  = 0x49E0,
    key_del   = 0x53E0,
    key_end   = 0x4FE0,
    key_pgdn  = 0x51E0
};

enum bioskey_00_t {
    key_num_7 = 0x4700,
    key_num_8 = 0x4800,
    key_num_9 = 0x4900,
    key_min_sub = 0x4A00,
    key_num_4 = 0x4B00,
    key_num_5 = 0x4C00,
    key_num_6 = 0x4D00,
    key_min_plu = 0x4E00,
    key_num_1 = 0x4F00,
    key_num_2 = 0x5000,
    key_num_3 = 0x5100,
    key_num_0 = 0x5200,
    key_num_dot = 0x5300,
    key_F1 = 0x3B00,
    key_F10 = 0x4400,
    key_F11 = 0x8500,
    key_F12 = 0x8600,

    key_num_first = key_num_7,
    key_num_last = key_num_dot
};
