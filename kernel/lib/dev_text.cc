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
    bool init() override final
    {
        return true;
    }
    void cleanup() override final
    {
    }
    int set_dimensions(int width _unused, int height _unused) override final
    {
        return 0;
    }

    void get_dimensions(int * width _unused,
                        int * height _unused) override final
    {
    }

    void goto_xy(int x _unused, int y _unused) override final
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
    void fg_set(int color _unused) override final
    {
    }
    int fg_get() override final
    {
        return 0;
    }
    void bg_set(int color _unused) override final
    {
    }
    int bg_get() override final
    {
        return 0;
    }
    int cursor_toggle(int show _unused) override final
    {
        return 0;
    }
    int cursor_is_shown() override final
    {
        return 0;
    }
    void putc(int character _unused) override final
    {
    }
    void putc_xy(int x _unused, int y _unused,
                 int character _unused) override final
    {
    }
    int print(char const * s _unused) override final
    {
        return 0;
    }
    int write(char const * s _unused, intptr_t len _unused) override final
    {
        return 0;
    }
    int print_xy(int x _unused, int y _unused,
                 char const * s _unused) override final
    {
        return 0;
    }
    int draw(char const *s _unused) override final
    {
        return 0;
    }
    int draw_xy(int x _unused, int y _unused,
                char const * s _unused, int attrib _unused) override final
    {
        return 0;
    }
    void fill(int sx _unused, int sy _unused,
              int ex _unused, int ey _unused,
              int character _unused) override final
    {
    }

    void clear() override final
    {
    }
    void scroll(int sx _unused, int sy _unused,
                int ex _unused, int ey _unused,
                int xd _unused, int yd _unused,
                int clear _unused) override final
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
    void mouse_goto_xy(int x _unused, int y _unused) override final
    {
    }
    void mouse_add_xy(int x _unused, int y _unused) override final
    {
    }
    int mouse_toggle(int show _unused) override final
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
    printk("Registered %s text device factory\n", name);
}

_constructor(ctor_text_dev)
static void text_dev_start()
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

        printk("text device registered\n");
    } else {
        console_display = &null_text_dev::instance;
    }
}

//REGISTER_CALLOUT(text_dev_start, nullptr, callout_type_t::txt_dev, "000");
