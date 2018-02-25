#pragma once
#include "types.h"

struct mouse_raw_event_t {
    int16_t hdist;
    int16_t vdist;
    int16_t wdist;
    int16_t buttons;
};

// negative hdist is left movement
// negative vdist is down movement

// buttons field
#define MOUSE_LMB       0x01
#define MOUSE_RMB       0x02
#define MOUSE_MMB       0x04
#define MOUSE_BACK      0x08
#define MOUSE_FORWARD   0x10
#define MOUSE_BUTTON(n) (1 << n)

// Used by mouse drivers to report mouse activity
void mouse_event(mouse_raw_event_t event);
