#include <ncurses.h>
#include <cstdio>
#include <cinttypes>
#include <map>
#include <string>
#include <algorithm>
#include <memory>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <cstring>
#include <linux/limits.h>

#include "../kernel/device/eainstrument.h"

#define POLL_DEBUG 0
#if POLL_DEBUG
#define POLL_TRACE(...) fprintf(__VA_ARGS__)
#else
#define POLL_TRACE(...) ((void)0)
#endif

using sym_tab = std::multimap<uint64_t, std::string>;

char symbol_file[] = "kernel-generic.sym";

std::unique_ptr<sym_tab> load_symbols()
{
    FILE *f = fopen(symbol_file, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", symbol_file);
        exit(1);
    }

    char line_buf[4096];

    std::unique_ptr<sym_tab> symbols(new sym_tab);

    while (fgets(line_buf, sizeof(line_buf), f)) {
        int addr_len;
        uint64_t addr;
        if (sscanf(line_buf, "%" PRIx64 " %n", &addr, &addr_len) < 1)
            continue;

        char *name = line_buf + addr_len;
        char *end = std::find(name, std::end(line_buf), '\n');

        symbols->emplace(addr, std::string(name, end));
    }

    printf("Found %zu symbols\n", symbols->size());

    fclose(f);

    return symbols;
}

void work()
{
    fprintf(stderr, "\nLoading symbols...");
    auto symbols = load_symbols();
    fprintf(stderr, "done\n");

    int notify = inotify_init1(IN_NONBLOCK);
    int watch = inotify_add_watch(notify, ".",
                                  /*IN_CREATE | IN_MODIFY |
                                  IN_DELETE |*/ IN_CLOSE_WRITE);

    int fd = open("dump/call-trace-out", O_EXCL);

    static constexpr int items_count = 1048576 / sizeof(trace_item);
    std::unique_ptr<trace_item[]> items(new trace_item[items_count]);
    static constexpr int sizeof_items = sizeof(items[0]) * items_count;
    size_t ofs = 0;
    for (;;) {
        pollfd polls[2] = {};

        // Watch for the data input stream to be readable
        polls[0].fd = fd;
        polls[0].events = POLLIN;

        // Watch for the symbols file to change
        polls[1].fd = notify;
        polls[1].events = POLLIN | POLLPRI;

        // Wait for something to happen
        POLL_TRACE(stderr, "Waiting...\n");
        int nevents = poll(polls, 2, -1);
        if (nevents < 0) {
            fprintf(stderr, "poll failed: %s\n", strerror(errno));
            exit(2);
        }

        if (polls[1].revents & POLLIN) {
            POLL_TRACE(stderr, "Reading notify event\n");
            union {
                inotify_event event{};
                char filler[sizeof(struct inotify_event) + NAME_MAX + 1];
            } u;
            memset(u.filler, 0, sizeof(u.filler));
            int got = read(notify, (char*)&u.event, sizeof(u));
            if (got < 0) {
                fprintf(stderr, "Filesystem watch read failed: %s\n",
                        strerror(errno));
                exit(3);
            }
            u.event.name[NAME_MAX] = 0;

            POLL_TRACE(stderr, "Notify name=%s\n", u.event.name);

            if (!strcmp(u.event.name, symbol_file)) {
                // Reload symbols
                fprintf(stderr, "\nreloading symbols...");
                symbols = load_symbols();
                fprintf(stderr, "done\n");
                continue;
            }
        }

        if (polls[0].revents & POLLHUP) {
            fprintf(stderr, "\nPipe hangup, reopening, nonblock\n");
            close(fd);
            fd = open("dump/call-trace-out", O_EXCL | O_NONBLOCK);
            POLL_TRACE(stderr, "\nReopen returned\n");
        }

        if (polls[0].revents & POLLIN) {
            POLL_TRACE(stderr, "Reading events...\n");
            int got = read(fd, (char*)items.get() + ofs, sizeof_items - ofs);
            if (got < 0 && errno == EINTR)
                continue;
            ofs += got;

            // Recover sync by shifting stream by one byte until sync is valid
            while (ofs >= sizeof(items[0]) && !items[0].valid())
                memmove(items.get(), (char*)items.get() + 1, --ofs);

            int got_count = ofs / sizeof(items[0]);
            ofs = ofs % sizeof(items[0]);

            for (int i = 0; i < got_count; ++i) {
                auto it = symbols->lower_bound(uint64_t(items[i].get_ip()));

                if (it != symbols->end()) {
                    printf("(c: %3d, t: %3d, I: %d) %s %s\n",
                           items[i].get_cid(), items[i].get_tid(),
                           items[i].irq_en,
                           items[i].call ? "->" : "<-",
                           it->second.c_str());
                } else {
                    printf("(c: %3d, t: %3d, I: %d) %s <??? @ %p>\n",
                           items[i].get_cid(), items[i].get_tid(),
                           items[i].irq_en,
                           items[i].call ? "->" : "<-",
                           items[i].get_ip());
                }
            }

            if (ofs)
                items[0] = items[got_count];
        }
    }
}

int main(int, char **)
{
    //initscr();
    //noecho();
    //keypad(stdscr, TRUE);
    //scrollok(stdscr,TRUE);
    //
    work();
    //
    //endwin();
    return 0;
}
