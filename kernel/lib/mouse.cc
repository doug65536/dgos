#include "mouse.h"
#include "conio.h"
#include "printk.h"

static int accum_x;
static int accum_y;

void mouse_event(mouse_raw_event_t event)
{
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
