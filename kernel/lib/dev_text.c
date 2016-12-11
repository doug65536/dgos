#include "dev_text.h"
#include "conio.h"
#include "printk.h"

void register_text_display_device(
        const char *name, text_display_vtbl_t *vtbl)
{
    text_display_base_t *devices;
    int device_count;

    device_count = vtbl->detect(&devices);

    if (device_count > 0)
        console_display = &devices[0];

    //
    // Store the default text display device where
    // printk can get at it

    console_display_vtbl = *console_display->vtbl;

    console_display_vtbl.clear(console_display);
    printk("%s device registered\n", name);
}
