#include "vt100_out.h"
#include "dev_text.h"
#include "serial-uart.h"

class vt100_out_factory_t : public text_dev_factory_t {
    // text_dev_factory_t interface
public:
    int detect(text_dev_base_t ***ptr_list) override final;
};

class vt100_out_t : public text_dev_base_t {
    TEXT_DEV_IMPL

    void attach(uart_dev_t *uart)
    {
        this->uart = uart;
    }

    ssize_t recv_csi(char *buf, size_t sz, int *params, size_t param_cnt);

    ssize_t send(char const *str, size_t sz);
    ssize_t send(char const *str);

    _printf_format(2, 0)
    ssize_t vsendf(char const *format, va_list ap);

    _printf_format(2, 3)
    ssize_t sendf(char const *format, ...);

    bool crsr_shown = true;
    uart_dev_t *uart = nullptr;
};

bool vt100_out_t::init()
{
    return true;
}

void vt100_out_t::cleanup()
{
}

int vt100_out_t::set_dimensions(int width, int height)
{
    return -int(errno_t::ENOSYS);
}

void vt100_out_t::get_dimensions(int *width, int *height)
{
    *width = 80;
    *height = 24;
}

void vt100_out_t::goto_xy(int x, int y)
{
    sendf("\x1b[%u;%uH", y + 1, x + 1);
}

int vt100_out_t::get_x()
{
    sendf("\x1b[6n");
    char buf[16];
    size_t buf_sz = 0;
    int params[2];
    ssize_t sz = recv_csi(buf, sizeof(buf), params, countof(params));

    return -int(errno_t::ENOSYS);
}

int vt100_out_t::get_y()
{
    return -int(errno_t::ENOSYS);
}

void vt100_out_t::fg_set(int color)
{
}

int vt100_out_t::fg_get()
{
    return -int(errno_t::ENOSYS);
}

void vt100_out_t::bg_set(int color)
{
}

int vt100_out_t::bg_get()
{
    return -int(errno_t::ENOSYS);
}

int vt100_out_t::cursor_toggle(int show)
{
    if (show)
        return send("\x1b" "?25h");

    return send("\x1b" "?25h");
}

int vt100_out_t::cursor_is_shown()
{
    return crsr_shown;
}

void vt100_out_t::putc(int character)
{
    uart->write(&character, 1);
}

void vt100_out_t::putc_xy(int x, int y, int character)
{
}

int vt100_out_t::print(char const *s)
{
    return -int(errno_t::ENOSYS);
}

int vt100_out_t::write(char const *s, intptr_t len)
{
    return -int(errno_t::ENOSYS);
}

int vt100_out_t::print_xy(int x, int y, char const *s)
{
    size_t tot = 0;
    ssize_t sz = send("\x1b" "7");
    if (unlikely(sz < 0))
        return sz;
    tot += sz;

    sz = send(s);
    if (unlikely(sz < 0))
        return sz;
    tot += sz;

    sz = send("\x1b" "8");
    if (unlikely(sz < 0))
        return sz;
    tot += sz;

    return tot;
}

int vt100_out_t::draw(char const *s)
{
    return -int(errno_t::ENOSYS);
}

int vt100_out_t::draw_xy(int x, int y,
                         char const *s, int attrib)
{
    return -int(errno_t::ENOSYS);
}

void vt100_out_t::fill(int sx, int sy,
                       int ex, int ey, int character)
{
}

void vt100_out_t::clear()
{
}

void vt100_out_t::scroll(int sx, int sy, int ex, int ey,
                         int xd, int yd, int clear)
{
}

int vt100_out_t::mouse_supported()
{
    return 0;
}

int vt100_out_t::mouse_is_shown()
{
    return 0;
}

int vt100_out_t::mouse_get_x()
{
    return -int(errno_t::ENOSYS);
}

int vt100_out_t::mouse_get_y()
{
    return -int(errno_t::ENOSYS);
}

void vt100_out_t::mouse_goto_xy(int x, int y)
{
}

void vt100_out_t::mouse_add_xy(int x, int y)
{
}

int vt100_out_t::mouse_toggle(int show)
{
    return -int(errno_t::ENOSYS);
}


ssize_t vt100_out_t::recv_csi(char *buf, size_t sz,
                              int *params, size_t param_cnt)
{
    size_t cur_param = 0;
    size_t buf_idx = 0;

    enum state_t {
        NOWHERE,
        IN_ESC,
        IN_CSI
    };

    state_t state = NOWHERE;
    size_t param_idx = 0;
    params[param_idx] = 0;

    for (;; ++buf_idx) {
        ssize_t rd = uart->read(buf + buf_idx, 1);

        if (unlikely(rd < 0))
            return rd;

        switch (state) {
        case NOWHERE:
            switch (buf[buf_idx]) {
            case '\x1b':
                state = IN_ESC;
                continue;
            case '\x9b':
                state = IN_CSI;
                continue;
            }
            break;

        case IN_ESC:
            switch (buf[buf_idx]) {
            case '[':
                state = IN_CSI;
                continue;
            }

            state = NOWHERE;
            continue;

        case IN_CSI:
            if (buf[buf_idx] >= '0' && buf[buf_idx] <= '9') {
                if (param_idx < param_cnt) {
                    params[param_idx] *= 10;
                    params[param_idx] += buf[buf_idx] - '0';
                }
                continue;
            } else if (buf[buf_idx] == ';') {
                if (++param_idx < param_cnt)
                    params[param_idx] = 0;
                continue;
            }

            return buf_idx + 1;
        }
    }
}

ssize_t vt100_out_t::send(const char *str, size_t sz)
{
    return uart->write(str, sz);
}

ssize_t vt100_out_t::send(const char *str)
{
    size_t sz = strlen(str);
    return send(str, sz);
}

ssize_t vt100_out_t::vsendf(char const *format, va_list ap)
{
    char buf[80];
    ssize_t sz = vsnprintf(buf, sizeof(buf), format, ap);
    return send(buf, sz);
}

ssize_t vt100_out_t::sendf(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ssize_t result = vsendf(format, ap);
    va_end(ap);
    return result;
}
