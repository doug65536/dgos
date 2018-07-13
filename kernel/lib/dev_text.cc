#include "dev_text.h"
#include "conio.h"
#include "printk.h"

static text_dev_factory_t *text_dev_factories;

class null_text_dev : public text_dev_base_t
{
public:
    static null_text_dev instance;

protected:
    // text_dev_base_t interface
    void init() override final
    {
    }
    void cleanup() override final
    {
    }
    int set_dimensions(int width, int height) override final
    {
        return 0;
    }
    void get_dimensions(int *width, int *height) override final
    {
    }
    void goto_xy(int x, int y) override final
    {
    }
    int get_x() override final
    {
        return 0;
    }
    int get_y() override final
    {
        return 0;
    }
    void fg_set(int color) override final
    {
    }
    int fg_get() override final
    {
        return 0;
    }
    void bg_set(int color) override final
    {
    }
    int bg_get() override final
    {
        return 0;
    }
    int cursor_toggle(int show) override final
    {
        return 0;
    }
    int cursor_is_shown() override final
    {
        return 0;
    }
    void putc(int character) override final
    {
    }
    void putc_xy(int x, int y, int character) override final
    {
    }
    int print(char const *s) override final
    {
        return 0;
    }
    int write(char const *s, intptr_t len) override final
    {
        return 0;
    }
    int print_xy(int x, int y, char const *s) override final
    {
        return 0;
    }
    int draw(char const *s) override final
    {
        return 0;
    }
    int draw_xy(int x, int y, char const *s, int attrib) override final
    {
        return 0;
    }
    void fill(int sx, int sy, int ex, int ey, int character) override final
    {
    }
    void clear() override final
    {
    }
    void scroll(int sx, int sy, int ex, int ey,
                int xd, int yd, int clear) override final
    {
    }
    int mouse_supported() override final
    {
        return 0;
    }
    int mouse_is_shown() override final
    {
        return 0;
    }
    int mouse_get_x() override final
    {
        return 0;
    }
    int mouse_get_y() override final
    {
        return 0;
    }
    void mouse_goto_xy(int x, int y) override final
    {
    }
    void mouse_add_xy(int x, int y) override final
    {
    }
    int mouse_toggle(int show) override final
    {
        return 0;
    }
};

null_text_dev null_text_dev::instance;

void register_text_display_device(
        char const *name, text_dev_factory_t *factory)
{
    factory->next_factory = text_dev_factories;
    text_dev_factories = factory;
}

void text_dev_start(void *)
{
    for (text_dev_factory_t *f = text_dev_factories; f; f = f->next_factory) {
        text_dev_base_t **ptrs;
        int count = f->detect(&ptrs);

        if (count && !console_display)
            console_display = ptrs[0];
    }

    //
    // Store the default text display device where
    // printk can get at it

    if (console_display) {
        console_display->clear();

        printk("vga device registered\n");
    } else {
        console_display = &null_text_dev::instance;
    }
}

REGISTER_CALLOUT(text_dev_start, nullptr, callout_type_t::txt_dev, "000");
