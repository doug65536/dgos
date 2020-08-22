// pci driver: C=DISPLAY, S=VGA

#include "kmodule.h"
#include "pci.h"
#include "user_mem.h"

PCI_DRIVER_BY_CLASS(
        vga,
        PCI_DEV_CLASS_DISPLAY,
        PCI_SUBCLASS_DISPLAY_VGA, -1);

#include "dev_text.h"
#include "fs/devfs.h"

#include "bios_data.h"
#include "mm.h"
#include "string.h"
#include "cpu/ioport.h"
#include "time.h"
#include "callout.h"

class vga_factory_t : public text_dev_factory_t, public zero_init_t {
public:
    constexpr vga_factory_t()
        : text_dev_factory_t("vga")
    {
    }

    int detect(text_dev_base_t ***ptrs) override final;
};

static vga_factory_t vga_factory;

class vga_conout_file_t : public dev_fs_file_t {
public:
    // dev_fs_file_t interface
    ssize_t read(char *buf, size_t size, off_t offset) override final
    {
        return -int(errno_t::ENOSYS);
    }

    ssize_t write(const char *buf, size_t size, off_t offset) override final
    {
        user_str_t text(buf, size, user_str_t::truncate_t());

        if (text)
            writedbg(text, text.lenof_str);

        return text.lenof_str;
    }


    // fs_file_info_t interface
public:
    ino_t get_inode() const override final
    {
        return -1;
    }
};

class vga_conout_reg_t : public dev_fs_file_reg_t {
public:
    vga_conout_reg_t() : dev_fs_file_reg_t("conout") {}

    // dev_fs_file_reg_t interface
    dev_fs_file_t *open(int flags, mode_t mode) override final
    {
        return new (ext::nothrow) vga_conout_file_t;
    }
};

class vga_conerr_reg_t : public dev_fs_file_reg_t {
public:
    vga_conerr_reg_t() : dev_fs_file_reg_t("conerr") {}

    // dev_fs_file_reg_t interface
    dev_fs_file_t *open(int flags, mode_t mode) override final
    {
        return new (ext::nothrow) vga_conout_file_t;
    }
};

// Aligned storage to defer construction
template<typename T>
using vga_reg_storage_t =
typename std::aligned_storage<sizeof(T), alignof(T)>::type;

static vga_reg_storage_t<vga_conout_reg_t> vga_conout_reg_storage;
static vga_reg_storage_t<vga_conerr_reg_t> vga_conerr_reg_storage;

static void vga_init_devs(void*)
{
    devfs_register(new (vga_conout_reg_storage.data) vga_conout_reg_t);
    devfs_register(new (vga_conerr_reg_storage.data) vga_conerr_reg_t);
}

REGISTER_CALLOUT(vga_init_devs, nullptr,
                 callout_type_t::devfs_ready, "000");

class text_display_t : public text_dev_base_t {
protected:
    virtual void remap() {}

    virtual void put_char_at(int x, int y, char32_t ch, int attr) = 0;

    virtual void move_hardware_cursor(int x, int y) = 0;

    static unsigned instance_count;

protected:

    friend class vga_factory_t;

    //uint16_t *video_mem;

    ioport_t io_base;

    int cursor_x;
    int cursor_y;
    int cursor_on;

    int width;
    int height;

    int attrib;

    int mouse_x;
    int mouse_y;
    int mouse_on;

    uint16_t *shadow;

    TEXT_DEV_IMPL

protected:
    void cap_position(int *px, int *py);
    int mouse_hide_if_at(int x, int y);
    int mouse_hide_if_within(int sx, int sy, int ex, int ey);
    void move_cursor_to(int x, int y);
    void move_cursor_if_on();
    void write_char_at(
            int x, int y,
            int character, int attrib);
    void force_char_at(
            int x, int y,
            int character, int attrib);
    void clear_screen();
    void fill_region(int sx, int sy, int ex, int ey, int character);
    void scroll_screen(int x, int y);
    void advance_cursor(int distance);
    void print_character(int ch);
    void print_characters_fast(char const *s, size_t len);

    friend void vga_remap_callback(void *);
};

class vga_display_t : public text_display_t {
    friend class vga_factory_t;

    void *video_mem = nullptr;

public:
    // This starts up extremely early, statically allocate
    static vga_display_t instances[1];

    void *operator new(size_t, ext::nothrow_t const&) noexcept
    {
        if (likely(instance_count < countof(instances)))
            return instances + instance_count++;
        return nullptr;
    }

    void operator delete(void *p) noexcept
    {
        if (p == instances + instance_count - 1)
            --instance_count;
        else {
            // lost instance because it isn't the last one!
            assert(0);
        }
    }

    // text_display_t interface
protected:
    void remap() override final;
    void move_hardware_cursor(int x, int y) override final;

    // text_display_t interface
protected:
    void put_char_at(int x, int y, char32_t ch, int attr) override final;
};

class framebuffer_display_t : public text_display_t {
    static framebuffer_display_t instances[1];

public:
    void *operator new(size_t, ext::nothrow_t const&) noexcept;

    void operator delete(void *p) noexcept;

    // text_display_t interface
protected:
    void move_hardware_cursor(int x, int y) override final;

    // text_display_t interface

    // text_display_t interface
protected:
    void put_char_at(int x, int y, char32_t ch, int attr) override final;
};

vga_display_t vga_display_t::instances[1];
framebuffer_display_t framebuffer_display_t::instances[1];
unsigned text_display_t::instance_count;

#define VGA_CRTC_PORT    (io_base)

#define VGA_CRTC_CURSOR_LO  0x0F
#define VGA_CRTC_CURSOR_HI  0x0E

#define VGA_SET_CURSOR_LO(pos) (VGA_CRTC_CURSOR_LO | \
    (((pos) << 8) & 0xFF00))

#define VGA_SET_CURSOR_HI(pos) (VGA_CRTC_CURSOR_HI | \
    ((pos) & 0xFF00))

// === Internal API ===

void text_display_t::cap_position(int *px, int *py)
{
    if (*px < 0)
        *px = 0;
    else if (*px >= width)
        *px = width - 1;

    if (*py < 0)
        *py = 0;
    else if (*py >= height)
        *py = height - 1;
}

int text_display_t::mouse_toggle(int show)
{
    if (!mouse_on != !show) {
        // Invert color at mouse
        size_t place = mouse_y *
                width +
                mouse_x;
        uint8_t attr = (shadow[place] >> 8) & 0xFF;
        uint8_t set_attr = show
                ? ((attr >> 4) | (attr << 4)) & 0x7F
                : attr;

        // Force visible if showing in cell with same color
        // for foreground and background
        if (show && ((set_attr >> 4) == (set_attr & 0x0F)))
            set_attr ^= 7 << 4;

        //write_char_at(mouse_x, mouse_y, shadow[place]);
        //video_mem[place] = (shadow[place] & 0xFF) | (set_attr << 8);
        mouse_on = !!show;
        return !show;
    }
    return !!show;
}

int text_display_t::mouse_hide_if_at(int x, int y)
{
    if (mouse_on &&
            mouse_x == x &&
            mouse_y == y) {
        return mouse_toggle(0);
    }
    return mouse_on;
}

int text_display_t::mouse_hide_if_within(int sx, int sy, int ex, int ey)
{
    if (mouse_on &&
            mouse_x >= sx && mouse_x < ex &&
            mouse_y >= sy && mouse_y < ey) {
        return mouse_toggle(0);
    }
    return mouse_on;
}

void text_display_t::move_cursor_to(int x, int y)
{
    move_hardware_cursor(x, y);
}

void text_display_t::move_cursor_if_on()
{
    if (cursor_on)
        move_cursor_to(cursor_x, cursor_y);
}

void text_display_t::write_char_at(int x, int y, int character, int attrib)
{
    int mouse_was_shown = mouse_hide_if_at(x, y);
    size_t place = y * width + x;
    uint16_t pair = (character & 0xFF) | ((attrib & 0xFF) << 8);
    shadow[place] = pair;
    //video_mem[place] = pair;
    mouse_toggle(mouse_was_shown);
}

void text_display_t::force_char_at(int x, int y, int character, int attrib)
{
    if (unlikely(y < 0 || x < 0 || x >= width || y >= height))
        return;

    int mouse_was_shown = mouse_hide_if_at(x, y);
    size_t place = y * width + x;
    uint16_t pair = (character & 0xFF) | ((attrib & 0xFF) << 8);
    //video_mem[place] = pair;
    mouse_toggle(mouse_was_shown);
}

void text_display_t::clear_screen()
{
    int mouse_was_shown = mouse_toggle(0);
    uint16_t *p = shadow;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            *p++ = uint16_t(' ' | (attrib << 8));
//    memcpy(video_mem, shadow,
//           width * height *
//           sizeof(*shadow));
    mouse_toggle(mouse_was_shown);
}

void text_display_t::fill_region(int sx, int sy, int ex, int ey, int character)
{
    int mouse_was_shown = mouse_hide_if_within(
                sx, sy, ex, ey);
    int row_count = ey - sy;
    int x;
    int row_ofs = (sy * width);
    uint16_t *shadow_row = shadow + row_ofs;
    //uint16_t *video_row = video_mem + row_ofs;

    character &= 0xFF;
    while (row_count--) {
        for (x = sx; x < ex; ++x)
            shadow_row[x] = character | (attrib << 8);

//        memcpy(video_row + sx,
//               shadow_row + sx,
//               sizeof(*video_mem) * (ex - sx));

        shadow_row += width;
//        video_row += width;
    }

    mouse_toggle(mouse_was_shown);
}

// negative x move the screen content left
// negative y move the screen content up
void text_display_t::scroll_screen(int x, int y)
{
    int row_size = width;
    int row_count;
    int row_step;
    int clear_start_row;
    int clear_end_row;
    int clear_start_col;
    int clear_end_col;
    uint16_t *src;
    uint16_t *dst;

    // Extreme move distance clears screen
    if ((x < 0 && -x >= width) ||
            (x > 0 && x >= width) ||
            (y < 0 && -y >= height) ||
            (y > 0 && y >= height)) {
        clear_screen();

        // Cursor tries to move with content

        cursor_x += x;
        cursor_y += y;

        cap_position(&cursor_x, &cursor_y);
        move_cursor_if_on();
        return;
    }

    if (y <= 0) {
        // Up
        src = shadow - (y * width);
        dst = shadow;
        row_count = height + y;
        // Loop from top to bottom
        row_step = width;
        // Clear the bottom
        clear_end_row = height;
        clear_start_row = clear_end_row + y;
    } else {
        // Down
        dst = shadow + ((height - 1) * width);
        src = shadow + ((height - y - 1) * width);
        row_count = height - y;
        // Loop from bottom to top
        row_step = -width;
        // Clear the top
        clear_start_row = 0;
        clear_end_row = y;
    }

    if (x <= 0) {
        // Left
        row_size += x;
        src -= x;
        // Clear right side
        clear_start_col = width + x;
        clear_end_col = width;
    } else {
        // Right
        row_size -= x;
        src += x;
        // Clear left side
        clear_start_col = 0;
        clear_end_col = x;
    }

    while (row_count--) {
        memmove(dst, src, row_size * sizeof(*dst));
        dst += row_step;
        src += row_step;
    }

    // Clear top/bottom
    if (clear_start_row != clear_end_row) {
        fill_region(
                    0,
                    clear_start_row,
                    width,
                    clear_end_row,
                    ' ');
    }

    // Clear left/right
    if (clear_start_col != clear_end_col) {
        fill_region(
                    clear_start_col,
                    clear_start_row ? clear_start_row : clear_end_row,
                    clear_end_col,
                    clear_start_row ? height : clear_start_row,
                    ' ');
    }

    int mouse_was_shown = mouse_toggle(0);
//    memcpy(video_mem, shadow,
//           width * height * sizeof(*shadow));
    mouse_toggle(mouse_was_shown);
}

// Advance the cursor, wrapping and scrolling as necessary.
// Does not update hardware cursor.
void text_display_t::advance_cursor(int distance)
{
    cursor_x += distance;
    if (cursor_x >= width) {
        cursor_x = 0;
        if (++cursor_y >= height) {
            cursor_y = height - 1;
            scroll_screen(0, -1);
        }
    }
}

// Handle linefeed and tab, advance cursor,
// put character on the screen, but does not
// update the hardware cursor
void text_display_t::print_character(int ch)
{
    int advance;
    switch (ch) {
    case '\n':
        advance_cursor(width - cursor_x);
        break;

    case '\r':
        advance_cursor(-cursor_x);
        break;

    case '\t':
        advance = ((cursor_x + 8) & -8) - cursor_x;
        advance_cursor(advance);
        break;

    case '\b':
        if (cursor_x > 0)
            advance_cursor(-1);
        break;

    default:
        write_char_at(cursor_x, cursor_y, ch, attrib);
        advance_cursor(1);
        break;
    }
}

// Print printable characters. Fragment must not wrap to next line.
void text_display_t::print_characters_fast(char const *s, size_t len)
{
    int mouse_was_shown = mouse_hide_if_within(
                cursor_x, cursor_y, cursor_x + len, cursor_y + 1);
    for (size_t place = cursor_y * width + cursor_x, end = place + len;
         place < end; ++place) {
        char character = *s++;
        uint16_t pair = (character & 0xFF) | ((attrib & 0xFF) << 8);
        shadow[place] = pair;
//        video_mem[place] = pair;
    }
    advance_cursor(len);
    mouse_toggle(mouse_was_shown);
}

// === Public API ===

int vga_factory_t::detect(text_dev_base_t ***ptrs)
{
    static text_dev_base_t *devs[countof(vga_display_t::instances)];

    if (text_display_t::instance_count >=
            countof(vga_display_t::instances)) {
        printdbg("Too many VGA devices!\n");
        return 0;
    }

    text_display_t* self = vga_display_t::instances +
            text_display_t::instance_count;

    *ptrs = devs + text_display_t::instance_count++;
    **ptrs = self;

    // Does not work (in qemu anyway)
    //pci_dev_iterator_t dev_iter;
    //if (!pci_enumerate_begin(
    //            &dev_iter,
    //            PCI_DEV_CLASS_DISPLAY,
    //            PCI_SUBCLASS_DISPLAY_VGA))
    //    return 0;


    if (!self->init())
        return 0;

    return 1;
}

// Startup/shutdown

// Starts statically with a pair of 80x25 buffers so they always exist
static constexpr size_t initial_video_width = 80;
static constexpr size_t initial_video_height = 25;
static constexpr size_t initial_video_count =
        initial_video_width * initial_video_height;
static uint16_t initial_video_ch[initial_video_count];
static uint16_t initial_video_shadow[initial_video_count];

bool text_display_t::init()
{
    // (Don't...) get I/O port base from BIOS data area
    io_base = 0x3B4;//*BIOS_DATA_AREA(uint16_t, BIOS_VGA_PORT_BASE);

    if (io_base == 0)
        io_base = 0x3B4;

    // Map frame buffer
    //video_mem = initial_video_ch;
    cursor_on = 1;
    cursor_x = 0;
    cursor_y = 0;
    width = initial_video_width;
    height = initial_video_height;
    attrib = 0x07;
    mouse_on = 0;
    mouse_x = width >> 1;
    mouse_y = height >> 1;

    // Off-screen shadow buffer
    shadow = initial_video_shadow;

    mouse_toggle(1);

//    devfs_register(&vga_conout_reg);
//    devfs_register(&vga_conerr_reg);

    return true;
}

void text_display_t::cleanup()
{
}

void vga_remap_callback(void *)
{
    for (text_display_t &self : vga_display_t::instances)
        self.remap();
}

void vga_display_t::remap()
{
    video_mem = (uint16_t*)
            mmap((void*)0xB8000, 0x8000,
                 PROT_READ | PROT_WRITE,
                 MAP_PHYSICAL | MAP_WRITETHRU);

    // Start using system RAM shadow buffer

    uint16_t *old_shadow = shadow;

    shadow = (uint16_t*)mmap(nullptr, width * height * sizeof(*shadow),
                             PROT_READ | PROT_WRITE, MAP_POPULATE);

    if (old_shadow) {
        memcpy(shadow, old_shadow,
               width * height * sizeof(*shadow));
        memset(old_shadow, 0,
               width * height * sizeof(*shadow));
    }
}

REGISTER_CALLOUT(vga_remap_callback, nullptr,
                 callout_type_t::vmm_ready, "000");

// Set/get dimensions

int text_display_t::set_dimensions(int width, int height)
{
    // Succeed if they didn't change it
    return width == this->width && height == this->height;
}

void text_display_t::get_dimensions(int *ret_width, int *ret_height)
{
    *ret_width = width;
    *ret_height = height;
}

// Set/get cursor position

void text_display_t::goto_xy(int x, int y)
{
    cap_position(&x, &y);

    if (cursor_x != x || cursor_y != y) {
        cursor_x = x;
        cursor_y = y;
        move_cursor_if_on();
    }
}

int text_display_t::get_x()
{
    return cursor_x;
}

int text_display_t::get_y()
{
    return cursor_y;
}

// Set/get foreground color

void text_display_t::fg_set(int color)
{
    attrib = (attrib & 0xF0) |
            (color & 0x0F);
}

int text_display_t::fg_get()
{
    return attrib & 0x0F;
}

// Set/get background color

void text_display_t::bg_set(int color)
{
    attrib = (attrib & 0x0F) |
            ((color & 0x0F) << 4);
}

int text_display_t::bg_get()
{
    return (attrib >> 4) & 0x0F;
}

// Show/hide cursor

int text_display_t::cursor_toggle(int show)
{
    int was_shown = cursor_on;

    if (!show != !cursor_on) {
        cursor_on = show;
        if (!show)
            move_cursor_to(0xFF, 0xFF);
        else
            move_cursor_to(cursor_x, cursor_y);
    }

    return was_shown;
}

int text_display_t::cursor_is_shown()
{
    return !!cursor_on;
}

// Print character, updating cursor position
// possibly scrolling the screen content up

void text_display_t::putc(int character)
{
    print_character(character);
    move_cursor_if_on();
}

void text_display_t::putc_xy(int x, int y, int character)
{
    cap_position(&x, &y);
    if (cursor_on &&
            (cursor_x != x || cursor_y != y))
        move_cursor_to(x, y);

    cursor_x = x;
    cursor_y = y;

    print_character(character);
}

int text_display_t::print(char const *s)
{
    int written = 0;
    while (*s) {
        ++written;
        print_character(*s++);
    }

    move_cursor_if_on();

    return written;
}

int text_display_t::write(char const *s, intptr_t len)
{
    int written = 0;
    while (len) {
        intptr_t fragment_sz = 0;
        while (fragment_sz < len &&
               s[fragment_sz] != '\n' &&
               s[fragment_sz] != '\t' &&
               s[fragment_sz] != '\r')
            ++fragment_sz;

        // Clamp to rest of line
        if (fragment_sz > width - cursor_x)
            fragment_sz = width - cursor_x;

        if (fragment_sz == 0) {
            // Non printable character
            ++written;
            print_character(*s++);
            --len;
        } else {
            print_characters_fast(s, fragment_sz);
            written += fragment_sz;
            s += fragment_sz;
            len -= fragment_sz;
        }
    }

    move_cursor_if_on();

    return written;
}

int text_display_t::print_xy(int x, int y, char const *s)
{
    goto_xy(x, y);
    return print(s);
}

// Draw text, no effect on cursor

int text_display_t::draw(char const *s)
{
    // FIXME
    (void)s;
    return 0;
}

int text_display_t::draw_xy(int x, int y, char const *s, int attrib)
{
    while (*s) {
        if (x >= width) {
            x = 0;
            if (++y >= height)
                y = 0;
        }

        write_char_at(x++, y, *s++, attrib);
    }
    return 0;
}

// Fill an area with a character, and optionally a color

void text_display_t::fill(int sx, int sy, int ex, int ey, int character)
{
    fill_region(sx, sy, ex, ey, character);
}

// Fill screen with spaces in the current colors
void text_display_t::clear()
{
    clear_screen();
}

// Scroll an area horizontally and/or vertically
// Optionally clearing exposed area

void text_display_t::scroll(int sx, int sy, int ex, int ey,
                           int xd, int yd, int clear)
{
    // FIXME
    (void)sx; (void)sy; (void)ex; (void)ey; (void)xd; (void)yd;
    (void)clear;
}


// Returns 0 if mouse is not supported
int text_display_t::mouse_supported()
{
    return 1;
}

// Show/hide mouse
int text_display_t::mouse_is_shown()
{
    return mouse_on;
}

// Get mouse position
int text_display_t::mouse_get_x()
{
    return mouse_x;
}

int text_display_t::mouse_get_y()
{
    return mouse_y;
}

// Move mouse to position
void text_display_t::mouse_goto_xy(int x, int y)
{
    int was_shown = mouse_toggle(0);
    cap_position(&x, &y);
    mouse_x = x;
    mouse_y = y;
    mouse_toggle(was_shown);
}

void text_display_t::mouse_add_xy(int x, int y)
{
    int was_shown = mouse_toggle(0);
    x += mouse_x;
    y += mouse_y;
    cap_position(&x, &y);
    mouse_x = x;
    mouse_y = y;
    mouse_toggle(was_shown);
}

void vga_display_t::move_hardware_cursor(int x, int y)
{
    uint16_t position = y * width + x;
    outw(VGA_CRTC_PORT, VGA_SET_CURSOR_LO(position));
    outw(VGA_CRTC_PORT, VGA_SET_CURSOR_HI(position));
}

void vga_display_t::put_char_at(int x, int y, char32_t ch, int attr)
{
}

void *
framebuffer_display_t::operator new(size_t, ext::nothrow_t const&) noexcept
{
    if (likely(instance_count < countof(instances)))
        return instances + instance_count++;
    return nullptr;
}

void framebuffer_display_t::operator delete(void *p) noexcept
{
    if (p == instances + instance_count - 1)
        --instance_count;
    else {
        // lost instance because it isn't the last one!
        assert(0);
    }
}

void framebuffer_display_t::move_hardware_cursor(int x, int y)
{

}

void framebuffer_display_t::put_char_at(int x, int y, char32_t ch, int attr)
{
}

#include "framebuffer.h"
