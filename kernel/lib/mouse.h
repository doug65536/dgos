#pragma once
#include "types.h"

__BEGIN_DECLS

struct mouse_raw_event_t {
    uint64_t timestamp;
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

// Ensure required memory is allocated before a mouse_event call occurs
KERNEL_API void mouse_file_init();

// Used by mouse drivers to report mouse activity
// It is illegal to call mouse_event before a call
// to mouse_file_init() has returned.
// Mouse input device drivers must complete at least one call to
// mouse_file_init before the driver can make a call to mouse_event.
KERNEL_API void mouse_event(mouse_raw_event_t event);

__END_DECLS
