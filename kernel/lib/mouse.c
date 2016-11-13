#include "mouse.h"
#include "printk.h"

void mouse_event(mouse_raw_event_t event)
{
    printk("Mouse h=%+4d v=%+4d w=%+2d b=%04x\n",
           event.hdist, event.vdist,
           event.wdist, event.buttons);
}
