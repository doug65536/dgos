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

    accum_x %= 45;
    accum_y %= 80;

    //if (accum_x >= 0)
    //    accum_x &= 7;
    //else
    //    accum_x = -((-accum_x) & 7);
    //
    //if (accum_y >= 0)
    //    accum_y &= 7;
    //else
    //    accum_y = -((-accum_y) & 7);

    //printk("Mouse h=%+4d v=%+4d w=%+2d b=%04x\n",
    //       event.hdist, event.vdist,
    //       event.wdist, event.buttons);
}
