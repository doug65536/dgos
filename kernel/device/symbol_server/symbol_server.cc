#include "kmodule.h"
#include "symbol_server.h"
#include "../serial-uart.h"
#include "fileio.h"
#include "main.h"
#include "elf64.h"
#include "cpu/perf.h"

class symbol_server_t {
    uart_dev_t *port = nullptr;
    thread_t tid = -1;

    unsigned scroll_left = 0;

    static int thread_entry(void *arg)
    {
        symbol_server_t *self = reinterpret_cast<symbol_server_t*>(arg);
        return self->worker();
    }

    ext::string base_name(char const *filename)
    {
        char const *end = filename + strlen(filename);
        char const *slash = strrchr(filename, '/');
        if (!slash)
            slash = filename;
        else
            ++slash;

        if ((end - slash) > 9 && !strcmp(end - 9, "-kallsyms"))
            end -= 9;

        std::vector<char> name(slash, end);

        for (char& ch: name) {
            if (likely((ch >= 'a' && ch <= 'z') ||
                       (ch >= 'A' && ch <= 'Z') ||
                       (ch >= '0' && ch <= '9')))
                continue;

            ch = '_';
        }

        return ext::string(name.begin(), name.end());
    }

    bool send_symbols_reply(long adj, char const *filename, bool with_name)
    {
        file_t syms_fid;

        syms_fid = file_openat(AT_FDCWD, filename, O_RDONLY);
        if (syms_fid < 0)
            return false;

        off_t sz = file_seek(syms_fid, 0, SEEK_END);
        if (sz < 0)
            return false;

        std::unique_ptr<char[]> buf(new (ext::nothrow) char[sz]);
        if (!buf)
            return false;

        off_t zero_pos = file_seek(syms_fid, 0, SEEK_SET);
        if (zero_pos < 0)
            return false;

        ssize_t did_read = file_read(syms_fid, buf.get(), sz);
        if (did_read < 0)
            return false;

        int close_result = syms_fid.close();
        if (close_result < 0)
            return false;

        // Parse the lines
        char *end = buf.get() + sz;
        char *next;

        ext::string module_name = base_name(filename);

        for (char *line = buf; line < end; line = next) {
            char *eol = (char*)memchr(line, '\n', end - line);
            eol = eol ? eol : end;
            next = eol + 1;

            char *addr_end;
            unsigned long addr = strtoul(line, &addr_end, 16);
            bool is_absolute = false;
            for (char *abc = addr_end; abc < eol; ++abc) {
                if (*abc == 'a' || *abc == 'A') {
                    is_absolute = true;
                    break;
                } else if (*abc != ' ') {
                    break;
                }
            }
            // Discard absolute symbols
            if (is_absolute)
                continue;

            char adjusted[17];
            ssize_t len = snprintf(adjusted, sizeof(adjusted),
                                   "%016lx", addr + (is_absolute ? 0 : adj));

            port->write(adjusted, len);
            port->write(addr_end, eol - addr_end);

            if (with_name) {
                port->wrstr(" [");
                port->write(module_name);
                port->write(']');
            }

            port->write("\r\n", 2);
        }

        return true;
    }

    static void perf_sample_callback(void *arg, int percent, int millipercent,
                                     char const *name)
    {
        symbol_server_t *self = (symbol_server_t*)arg;
        char buf[1024];
        int sz = snprintf(buf, sizeof(buf), "%3d.%02d%% %s\n",
                          percent, millipercent / 10, name);
        if (sz > 0)
            self->port->write(buf, size_t(sz));
    }

    static void perf_top_callback(void *arg, int percent, int millipercent,
                                  char const *name)
    {
        symbol_server_t *self = (symbol_server_t*)arg;
        unsigned &scroll_left = self->scroll_left;
        if (top_rows++ > 16)
            return;

        char edited_name[128];
        size_t name_len = strlen(name);
        if (name_len < 96) {
            // Do nothing
        } else {
            ssize_t offset = std::min(ssize_t(name_len - 96),
                                      ssize_t(scroll_left));

            name = strncpy(edited_name,
                   name + offset,
                   96);

            if (offset > 0)
                memcpy(edited_name, "...", 3);

            if (offset == scroll_left)
                memcpy(edited_name + 96 - 3, "...", 3);
        }

        char buf[128];
        int sz = snprintf(buf, sizeof(buf),
                          "\x1b" "[%zu;1H"
                          "\x1b" "[K"
                          "%3d.%02d%% %.96s\n",
                          top_rows,
                          percent, millipercent / 10, name);
        if (sz > 0)
            self->port->write(buf, size_t(sz));
    }

    void modal_top(uint64_t total_samples, char command)
    {
        port->write(ext::string(16, '\n'));

        for (bool done = false ; !done;) {
            // up 16 lines
            port->wrstr("\x1b" "[16A");

            top_rows = 0;
            total_samples = perf_gather_samples(perf_top_callback, this);

            while (top_rows < 16) {
                port->wrstr("\x1b" "[");
                port->write(ext::to_string(top_rows++));
                port->wrstr(";1H"
                            "\x1b" "[K");
            }

            port->wrstr("\r" "\x1b" "[K");
            port->write(ext::to_string(total_samples));
            port->wrstr(" samples\r\n");

            size_t cpu_count;
            cpu_count = thread_get_cpu_count();
            unsigned usage_1k_total = 0;
            for (size_t cpu_nr = 0; cpu_nr < cpu_count; ++cpu_nr) {
                unsigned usage_1k = thread_cpu_usage_x1k(cpu_nr);
                usage_1k_total += usage_1k;
            }
            usage_1k_total /= cpu_count;
            port->wrstr("\x1b" "[K");
            port->wrstr("CPU usage: ");

            unsigned usage_fixed = usage_1k_total / 1000;
            unsigned usage_frac = usage_1k_total % 1000;

            ext::string fixed = ext::to_string(usage_fixed);
            ext::string frac = ext::to_string(usage_frac);
            port->write(std::move(fixed));
            port->wrstr(".");
            port->write(std::move(frac));
            port->wrstr("%\r\n");

            char input = 0;
            uart_dev_t::clock::time_point now =
                    uart_dev_t::clock::now();
            uart_dev_t::clock::time_point timeout =
                    now + std::chrono::seconds(1);

            ssize_t read_sz = port->read(&input, 1, 1, timeout);

            if (read_sz == 1) {
                switch (input) {
                case 'q':
                    done = true;
                    port->wrstr("\r\x1b" "[K" "(symsrv) ");
                    break;

                case '\x1b':
                    read_sz = port->read(&command, 1, 1);
                    if (-(read_sz > 0) & (command != '['))
                        break;
                    read_sz = port->read(&command, 1, 1);
                    switch (-(read_sz > 0) & command) {
                    case 'D':
                        scroll_left -= scroll_left > 0;
                        break;
                    case 'C':
                        ++scroll_left;
                        break;
                    }
                    break;
                }
            }
        }
    }

    int worker()
    {
        printdbg("symsrv: started\n");

        for (;;) {
            char command = 0;
            int did_read = port->read(&command, 1, 1);

            if (did_read != 1)
                continue;

            printdbg("symsrv: got input\n");

            size_t mod_count = modload_get_count();

            switch (command) {
            case '\n':
                // telnet force into no echo character mode
                port->wrstr("\xff\xfb\x01\xff\xfb\x3\xff\xfc\x22");
                port->wrstr("(symsrv) ");
                break;

            case 's':
                // FIXME: pass through from bootloader
                char const *filename;
                filename = "sym/kernel-generic-kallsyms";

                // Dump symbols
                send_symbols_reply(-uintptr_t(__image_start), filename, true);

                // Dump each module symbols
                for (size_t i = 0; i < mod_count; ++i) {
                    module_t *m = modload_get_index(i);

                    ext::string name = modload_get_name(m);

                    ext::string symname;
                    symname.reserve(4 + name.length() + 9);
                    symname.append("sym/")
                            .append(name)
                            .append("-kallsyms");

                    send_symbols_reply(0, symname.c_str(), true);
                }

                break;

            case 'm':

                size_t kernel_sz;
                kernel_sz = kernel_get_size();

                port->wrstr("\r\nkernel_generic");
                port->write(' ');
                port->write(ext::to_string(kernel_sz));
                port->wrstr(" 0 - Live ");
                port->write(ext::to_hex(uintptr_t(__image_start)));

                for (size_t i = 0; i < mod_count; ++i) {
                    module_t *m = modload_get_index(i);

                    ext::string name = modload_get_name(m);
                    ptrdiff_t base = modload_get_base(m);
                    size_t size = modload_get_size(m);

                    port->wrstr("\r\n");
                    port->write(base_name(name.c_str()));
                    port->write(' ');
                    port->write(ext::to_string(size));
                    port->wrstr(" 0 - Live 0x");

                    char addr_text[17];
                    snprintf(addr_text, sizeof(addr_text), "%zx", base);

                    port->write(addr_text, 16);
                }

                break;

            case 'p':
                uint64_t total_samples;
                total_samples = perf_gather_samples(perf_sample_callback, this);
                port->write(ext::to_string(total_samples));
                port->wrstr(" samples\n");
                break;

            case 'e':
                size_t event;
                char event_char;
                event = 0;
                event_char = 0;
                for (;;) {
                    port->read(&event_char, 1);
                    if (event_char >= '0' && event_char <= '9') {
                        event <<= 4;
                        event |= event_char - '0';
                    } else if (event_char >= 'A' && event_char <= 'F') {
                        event <<= 4;
                        event |= 10 + event_char - 'A';
                    } else if (event_char >= 'a' && event_char <= 'f') {
                        event <<= 4;
                        event |= 10 + event_char - 'a';
                    } else {
                        break;
                    }
                }
                perf_set_event(event >> 8, event & 0xF);
                break;

            case 't':
                modal_top(total_samples, command);

                break;

            }
        }
    }

public:
    symbol_server_t()
    {
        //perf_init();

        port = uart_dev_t::open(uart_dev_t::com[3], 5,
                115200, 8, 'N', 1, false);

        tid = thread_create(&symbol_server_t::thread_entry, this,
                            "symbol_server", 0, false, false);
    }

    static size_t top_rows;
};

size_t symbol_server_t::top_rows;

symbol_server_t instance;
