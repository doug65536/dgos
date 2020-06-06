#include "mouse.h"
#include "conio.h"
#include "printk.h"
#include "export.h"

#define DEBUG_MOUSE 1
#if DEBUG_MOUSE
#define MOUSE_TRACE(...) printdbg("mouse: " __VA_ARGS__)
#else
#define MOUSE_TRACE(...) ((void)0)
#endif

static int accum_x;
static int accum_y;

EXPORT void mouse_event(mouse_raw_event_t event)
{
    MOUSE_TRACE("hdist=%+d, vdist=%+d, buttons=0x%x\n",
                event.hdist, event.vdist, event.buttons);

    accum_x += event.hdist * 10;
    accum_y += -event.vdist * 10;

    con_move_cursor(accum_x / 45, accum_y / 80);

    // Improve responsiveness by resetting accumulator
    // when mouse reverses direction on that axis
    if ((accum_x < 0) != (event.hdist < 0))
        accum_x = 0;
    if ((accum_y < 0) != (-event.vdist < 0))
        accum_y = 0;

    accum_x %= 45;
    accum_y %= 80;
}
