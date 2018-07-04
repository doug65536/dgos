//#include <ncurses.h>
#include <cstdio>
#include <cinttypes>
#include <map>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <memory>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <cstring>
#include <linux/limits.h>
#include <zlib.h>
#include <thread>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <csignal>
#include <exception>
#include <cassert>

#include "../kernel/device/eainstrument.h"

#define POLL_DEBUG 0
#if POLL_DEBUG
#define POLL_TRACE(...) fprintf(__VA_ARGS__)
#else
#define POLL_TRACE(...) ((void)0)
#endif

#if defined(__GNUC__) && !defined(likely) && !defined(unlikely)
#define likely(expr) __builtin_expect(!!(expr), 1)
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#else
#undef likely(expr)
#undef unlikely(expr)
#define likely(expr) (!!(expr))
#define unlikely(expr) (!!(expr))
#endif

using sym_tab = std::multimap<uint64_t, std::string>;

using trace_item_vector = std::vector<trace_item>;

char symbol_file[] = "kernel-generic.sym";

class trace_error : public std::runtime_error {
public:
    trace_error(std::string message, error_t err)
        : std::runtime_error(message + strerror(err))
    {
    }
};

class writer_thread_t {
public:
    using buffer_t = trace_item_vector;

    writer_thread_t()
        : stop(false)
        , out_file(nullptr)
    {
        thread = std::thread(&writer_thread_t::start, this);
    }

    ~writer_thread_t()
    {
        lock_hold hold(lock);
        stop = true;
        wake.notify_one();
        hold.unlock();
        thread.join();
    }

    void enqueue(buffer_t&& buffer)
    {
        if (unlikely(caught_exception))
            std::rethrow_exception(caught_exception);

        if (likely(!stop)) {
            lock_hold hold(lock);
            buffer_queue.emplace_back(std::move(buffer));
            wake.notify_one();
        }
    }

    void reset()
    {
        lock_hold hold(lock);

        if (unlikely(caught_exception))
            std::rethrow_exception(caught_exception);

        if (out_file)
            close_output_file(hold);

        buffer_queue.clear();

        open_output_file(hold);
    }

private:
    void start()
    {
        try {
            write_worker();
        } catch (...) {
            caught_exception = std::current_exception();
        }
    }

    void open_output_file(std::unique_lock<std::mutex>&)
    {
        out_file = gzopen64("calltrace.dmp.gz", "w");

        if (unlikely(!out_file))
            throw trace_error("Could not open output file", errno);
    }

    void close_output_file(std::unique_lock<std::mutex>&)
    {
        if (out_file) {
            if (gzclose(out_file) != Z_OK) {
                error_t err = errno;

                if (std::uncaught_exception()) {
                    fprintf(stderr, "warning: gzclose failed"
                                    " during exception unwinding: %s\n",
                            strerror(err));
                } else {
                    throw trace_error("gzclose failed: ", err);
                }
            }
            out_file = nullptr;
        }
    }

    void write_worker()
    {
        lock_hold hold(lock);
        open_output_file(hold);
        hold.unlock();

        std::vector<buffer_t> buffers;

        bool flushed = true;

        for (;;) {
            // Drain the pending buffers queue
            hold.lock();
            while (!stop && buffer_queue.empty()) {
                int64_t timeout_duration = flushed
                        ? std::numeric_limits<uint64_t>::max()
                        : 2000;
                std::chrono::milliseconds timeout{timeout_duration};
                if (wake.wait_for(hold, timeout) == std::cv_status::timeout) {
                    gzflush(out_file, 0);
                    flushed = true;
                }
            }

            if (stop)
                break;

            // Move all of the buffers into our local vector and release lock
            buffers.reserve(buffer_queue.size());
            std::move(std::begin(buffer_queue), std::end(buffer_queue),
                      std::back_inserter(buffers));
            buffer_queue.clear();
            hold.unlock();

            // Write all the buffers
            for (buffer_t const& buffer : buffers) {
                unsigned write_size = buffer.size() *
                        sizeof(*buffer.data());
                int wrote = gzwrite(out_file, buffer.data(), write_size);
                if (unlikely(wrote != write_size))
                    throw trace_error("gzwrite failed: ", errno);

                flushed = false;
            }

            buffers.clear();
        }

        assert(hold.owns_lock());
        close_output_file(hold);
    }

    std::thread thread;
    std::mutex lock;
    std::condition_variable wake;
    std::vector<buffer_t> buffer_queue;
    using lock_hold = std::unique_lock<std::mutex>;
    gzFile out_file;
    bool stop;
    std::exception_ptr caught_exception;
};

struct thread_state {
    thread_state()
        : indent(0)
    {
    }

    int indent;
};

using thread_tab = std::unordered_map<int, thread_state>;

std::unique_ptr<sym_tab> load_symbols()
{
    FILE *f = fopen(symbol_file, "r");
    if (unlikely(!f))
        throw trace_error(std::string("Cannot open ") + symbol_file, errno);

    char line_buf[4096];

    std::unique_ptr<sym_tab> symbols(new sym_tab);

    while (likely(fgets(line_buf, sizeof(line_buf), f))) {
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

sig_atomic_t quit;

void sigint_handler(int)
{
    quit = true;
}

int capture()
{
    signal(SIGINT, sigint_handler);

    fprintf(stderr, "\nLoading symbols...");
    auto symbols = load_symbols();
    fprintf(stderr, "done\n");

    thread_tab threads;

    int notify = inotify_init1(IN_NONBLOCK);
    if (unlikely(notify < 0))
        throw trace_error("could not create inotify: %s\n", errno);

    int watch = inotify_add_watch(notify, ".", IN_CLOSE_WRITE);
    if (unlikely(watch < 0))
        throw trace_error("could not create inofity watch: %s\n", errno);

    int in_fd = open("dump/call-trace-out", O_EXCL);
    if (unlikely(in_fd < 0))
        throw trace_error("could not open call trace FIFO\n", errno);

    writer_thread_t writer;

    static constexpr int items_count = 1048576 / sizeof(trace_item);
    std::unique_ptr<trace_item[]> items(new trace_item[items_count]);
    static constexpr int sizeof_items = sizeof(items[0]) * items_count;
    size_t ofs = 0;
    while (!quit) {
        pollfd polls[2];

        // Watch for the data input stream to be readable
        polls[0].fd = in_fd;
        polls[0].events = POLLIN;

        // Watch for the symbols file to change
        polls[1].fd = notify;
        polls[1].events = POLLIN;

        // Wait for something to happen
        POLL_TRACE(stderr, "Waiting...\n");
        int nevents = poll(polls, 2, -1);
        if (unlikely(nevents < 0))
            throw trace_error("poll failed: ", errno);

        if (unlikely(polls[1].revents & POLLIN)) {
            // A file changed

            POLL_TRACE(stderr, "Reading notify event\n");
            union {
                inotify_event event{};
                char filler[sizeof(struct inotify_event) + NAME_MAX + 1];
            } u;
            memset(u.filler, 0, sizeof(u.filler));
            int got = read(notify, (char*)&u.event, sizeof(u));
            if (got < 0)
                throw trace_error("filesystem watch read failed: ", errno);

            u.event.name[NAME_MAX] = 0;

            POLL_TRACE(stderr, "Notify name=%s\n", u.event.name);

            if (!strcmp(u.event.name, symbol_file)) {
                // Reload symbols
                fprintf(stderr, "\nReloading symbols...");
                symbols = load_symbols();
                fprintf(stderr, "done\n");
                continue;
            }
        }

        if (unlikely(polls[0].revents & POLLHUP)) {
            // The writer end of the FIFO (qemu) closed their stream

            fprintf(stderr, "\nPipe hangup, reopening, nonblock\n");
            close(in_fd);
            in_fd = open("dump/call-trace-out", O_EXCL | O_NONBLOCK);
            POLL_TRACE(stderr, "\nReopen returned\n");

            // Everything we knew about the nesting level is potentially wrong
            threads.clear();
        }

        if (likely(polls[0].revents & POLLIN)) {
            // Event FIFO is ready to read

            POLL_TRACE(stderr, "Reading events...\n");
            int got = read(in_fd, (char*)items.get() + ofs, sizeof_items - ofs);
            if (got < 0 && errno == EINTR)
                continue;
            ofs += got;

            // Recover sync by shifting stream by one byte until sync is valid
            while (unlikely(ofs >= sizeof(items[0]) && !items[0].valid())) {
                fprintf(stderr, "Recovering sync!\n");
                memmove(items.get(), (char*)items.get() + 1, --ofs);
            }

            // Compute how many full events we got,
            int got_count = ofs / sizeof(items[0]);
            // and account for any partially read ones at the end
            ofs = ofs % sizeof(items[0]);

            writer.enqueue(trace_item_vector(items.get(),
                                             items.get() + got_count));

            for (int i = 0; i < got_count; ++i) {
                trace_item const& item = items[i];

                if (unlikely(!item.valid())) {
                    // Synchronization lost, resynchronize
                    fprintf(stderr, "Recovering sync!\n");
                    char *st = (char*)items.get();
                    char *en = (char*)(items.get() + got_count) + ofs;
                    ofs = en - st - 1;
                    memmove(st, st + 1, ofs);
                    got_count = 0;
                    break;
                }

                int tid = item.get_tid();

                if (!tid)
                    tid = item.get_cid();

                thread_state& thread = threads[tid];

                if (!item.call && thread.indent > 0)
                    --thread.indent;

                // Lookup symbol from instruction pointer
                auto it = symbols->lower_bound(uint64_t(item.get_ip()));

                if (likely(it != symbols->end())) {
                    printf("(c: %3d, t: %3d, I: %d) %*s %s %s\n",
                           item.get_cid(), item.get_tid(),
                           item.irq_en,
                           thread.indent, "",
                           item.call ? "->" : "<-",
                           it->second.c_str());
                } else {
                    printf("(c: %3d, t: %3d, I: %d) %*s %s <??? @ %p>\n",
                           item.get_cid(), item.get_tid(),
                           item.irq_en,
                           thread.indent, "",
                           item.call ? "->" : "<-",
                           item.get_ip());
                }

                thread.indent += (int)item.call;
            }

            if (unlikely(ofs && got_count))
                items[0] = items[got_count];
        }
    }
}

int main(int, char **)
{

int main(int argc, char **argv)
{

    //initscr();
    //noecho();
    //keypad(stdscr, TRUE);
    //scrollok(stdscr,TRUE);
    //
    try {
        if (argc == 1)
            capture();
    } catch (std::exception const& ex) {
        fprintf(stderr, "Error: %s\n", ex.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "Fatal error\n");
        return 1;
    }
    //
    //endwin();
    return 0;
}
