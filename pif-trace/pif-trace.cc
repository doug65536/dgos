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
#include <regex>
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

char symbol_file[] = "kernel-tracing.sym";

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
            std::unique_lock<std::mutex> hold(lock);
            close_output_file(hold);
        } catch (...) {
            caught_exception = std::current_exception();
            std::unique_lock<std::mutex> hold(lock);
            close_output_file(hold);
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
                    gzflush(out_file, Z_FINISH);
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

    // 1048576 should be enough to completely drain the FIFO in one read call
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

// Include down here because of ridiculous
// #define namespace pollution
#include <ncurses.h>

struct alignas(16) trace_detail {
    // The record from the trace log
    trace_record rec;

    // Indicates the order of occurence in the trace
    uint64_t ordinal:48;

    // The indent level at this point
    uint16_t indent:16;

    bool operator<(uint64_t n) const
    {
        return ordinal < n;
    }
} _packed;

static_assert(sizeof(trace_detail) == 16, "Unexpected size");

using trace_detail_vector = std::vector<trace_detail>;
using trace_detail_iter = trace_detail_vector::iterator;
using trace_thread_map = std::unordered_map<int, trace_detail_vector>;

trace_thread_map load_trace(char const *filename)
{
    trace_thread_map thread_map;

    trace_item_vector buffer;

    static constexpr int buffer_count = 1048576 / sizeof(trace_item);
    static constexpr int buffer_size = buffer_count * sizeof(trace_item);

    gzFile file = gzopen64(filename, "r");

    uint64_t ordinal = 0;

    buffer.resize(buffer_count);
    for (;;) {
        int got = gzread(file, buffer.data(), buffer_size);
        if (unlikely(got < 0))
            throw trace_error("Error reading input file", errno);

        if (got == 0)
            break;

        int got_count = got / sizeof(trace_item);

        for (auto it = buffer.begin(), en = buffer.begin() + got_count;
             it != en; ++it) {
            trace_item const& item = *it;

            int tid = item.get_tid();

            if (!tid)
                tid = item.get_cid();

            trace_detail_vector &detail_vector = thread_map[tid];

            trace_detail prev = !detail_vector.empty()
                    ? detail_vector.back()
                    : trace_detail{};

            uint16_t indent;
            if (prev.rec.call && item.call)
                indent = prev.indent + 1;
            else if (!prev.rec.call && !item.call && prev.indent > 0)
                indent = prev.indent - 1;
            else
                indent = prev.indent;

            trace_detail rec{ trace_record(item),
                        ordinal++, indent };

            detail_vector.push_back(rec);
        }
    }

    for (trace_thread_map::value_type& thread_data : thread_map) {
        thread_data.second.shrink_to_fit();

        for (auto it = thread_data.second.begin(),
             en = thread_data.second.end(); it != en; ++it) {
            assert(it->rec.show == it->rec.showable);

            if (it + 1 != en) {
                it->rec.expandable = it[1].indent > it->indent;
                it->rec.expanded = it->rec.expandable;

                // Hide the return line for leaf calls
                if (it[0].rec.call && !it[1].rec.call &&
                        it[0].rec.fn == it[1].rec.fn &&
                        it[0].indent == it[1].indent) {
                    it[1].rec.showable = false;
                    it[1].rec.show = false;
                }
            }

            assert(it->rec.show == it->rec.showable);
        }

        fprintf(stderr, "tid %u, %zu records\n",
                thread_data.first, thread_data.second.size());
    }

    return thread_map;
}

static bool is_detail_shown(trace_detail const& item)
{
    return item.rec.show;
}

class viewer {
public:
    viewer(char const *filename)
        : filename(filename)
        , symbols(load_symbols())
        , data(load_trace(filename))
        , done(false)
        , tid(0)
        , tid_detail(&data[tid])
        , offset(0)
        , cursor_row(0)
    {
        initscr();
        noecho();
        keypad(stdscr, TRUE);
        mouseinterval(0);
        mousemask(BUTTON1_PRESSED, nullptr);
        scrollok(stdscr, FALSE);
        curs_set(FALSE);
        nodelay(stdscr, FALSE);
        start_color();
        use_default_colors();
        init_pair(2, COLOR_WHITE, -1);
        wbkgdset(stdscr, COLOR_PAIR(2));
        timeout(-1);
    }

    ~viewer()
    {
        endwin();
    }

    void run()
    {
        do {
            draw();
            read_input();
        } while (!done);
    }

    void draw()
    {
        erase();

        getmaxyx(stdscr, display_h, display_w);

        trace_detail_iter item = update_selection();

        update_highlight_range();

        init_pair(1, COLOR_YELLOW, -1);

        for (int y = 0; y < display_h && item != tid_detail->end();
             ++y, ++item) {
            // Find visible item
            item = std::find_if(item, tid_detail->end(), is_detail_shown);

            // If this is the top row, adjust offset
            if (y == 0)
                offset = std::distance(tid_detail->begin(), item);

            trace_record const& rec = item->rec;
            uint64_t ip = uint64_t(rec.get_ip());
            auto it = symbols->lower_bound(ip);

            if (y <= cursor_row)
                selection = item;

            if (y == cursor_row) {
                attron(A_STANDOUT);
            } else {
                attroff(A_STANDOUT);
            }

            if (item >= highlight_st && item <= highlight_en) {
                attron(COLOR_PAIR(1));
                //attron(A_BOLD);
            } else {
                attroff(COLOR_PAIR(1));
                //attroff(A_BOLD);
            }

            char const *tree_widget;
            if (rec.expandable) {
                if (rec.expanded)
                    tree_widget = "->";
                else
                    tree_widget = "+>";
            } else {
                if (rec.call)
                    tree_widget = " >";
                else
                    tree_widget = " <";
            }

            mvprintw(y, 0, "(c: %3d, t: %3d, I: %d) %*s %s %s\n",
                     rec.get_cid(), tid,
                     rec.irq_en,
                     item->indent * 2, "",
                     tree_widget,
                     it->second.c_str());
        }

        refresh();
    }

    void update_highlight_range()
    {
        highlight_st = tid_detail->end();
        highlight_en = tid_detail->end();

        if (unlikely(selection == tid_detail->end()))
            return;

        int indent = selection->indent;

        highlight_st = selection;

        // If on a return, scan back to corresponding call
        if (!highlight_st->rec.call) {
            while (highlight_st != tid_detail->begin()) {
                --highlight_st;

                if (highlight_st->indent == indent)
                    break;
            }
        }

        highlight_en = highlight_st + 1;

        while (highlight_en != tid_detail->end() &&
               highlight_en->indent != indent)
            ++highlight_en;
    }

    trace_detail_iter update_selection()
    {
        offset = std::min(int64_t(tid_detail->size()), offset);
        trace_detail_iter item = tid_detail->begin();
        selection = item;
        std::advance(item, offset);

        for (int y = 0; y < display_h && item != tid_detail->end();
             ++y, ++item) {
            // Find visible item
            item = std::find_if(item, tid_detail->end(), is_detail_shown);

            // If this is the top row, adjust offset
            if (y == 0)
                offset = std::distance(tid_detail->begin(), item);

            if (y <= cursor_row)
                selection = item;
            else
                break;
        }

        return tid_detail->begin() + offset;
    }

    void read_input()
    {
        int key = getch();

        switch (key) {
        case 't':   // fall through
        case KEY_HOME:
            return move_home();

        case 'b':   // fall through
        case KEY_END:
            return move_end();

        case KEY_UP:
            return move_up();

        case KEY_F(7):   // fall through
        case KEY_F(11):   // fall through
        case KEY_DOWN:
            return move_down();

        case '-':   // fall through
        case KEY_LEFT:
            return tree_collapse();

        case '+':   // fall through
        case KEY_RIGHT:
            return tree_expand();

        case KEY_NPAGE:
            return page_down();

        case KEY_PPAGE:
            return page_up();

        case '[':
            return scroll_view(-1);

        case ']':
            return scroll_view(1);

        case KEY_F(8):  // fall through
        case KEY_F(10):
            return move_next();

        case '*':
            return tree_expand_all();

        case 'u':
            return move_caller();

        case 'e':
            return elide_call();

        case '/':
            return find_text();

        case 'n':
            return find_next(1);

        case 'N':
            return find_next(-1);

        case 'h':
            return show_help();

        case KEY_RESIZE:
            return handle_resize();

        case KEY_MOUSE:
            return handle_mouse();

        case 'q':
            done = true;
            break;
        }
    }

    // Only returns end when nothing at all is shown
    // Tries to backtrack in the other direction to find a shown item
    trace_detail_iter advance_shown(trace_detail_iter it, int distance)
    {
        bool full_scan;
        trace_detail_iter limit;
        int dir = (distance > 0 ? 1 : -1);

        if (dir > 0) {
            limit = tid_detail->end();
            full_scan = (it == tid_detail->begin());
        } else {
            limit = tid_detail->begin();
            full_scan = (it == tid_detail->end());
        }

        int seen = 0;
        while (it != limit) {
            it += dir;

            if (it != tid_detail->end() && is_detail_shown(*it)) {
                seen += dir;

                if (seen == distance)
                    break;
            }
        }

        if (likely(it != tid_detail->end() && is_detail_shown(*it)))
            return it;

        // Prevent infinite recursion
        if (unlikely(full_scan))
            return tid_detail->end();

        // Backtrack if we never reached a visible item
        return advance_shown(it, -dir);
    }

    int visible_between(trace_detail_iter st, trace_detail_iter en)
    {
        int count;
        for (count = 0; st != en; ++st)
            count += is_detail_shown(*st);

        return count;
    }

    void clamp_offset()
    {
        trace_detail_iter min_offset = advance_shown(
                    selection, -(display_h - 1));
        trace_detail_iter cur_offset = tid_detail->begin() + offset;

        if (cur_offset > selection)
            offset = selection - tid_detail->begin();
        else if (cur_offset < min_offset)
            offset = min_offset - tid_detail->begin();

        cur_offset = tid_detail->begin() + offset;

        cursor_row = visible_between(cur_offset, selection);
    }

    void move_home()
    {
        offset = 0;
        cursor_row = 0;
    }

    void move_end()
    {
        selection = advance_shown(tid_detail->end(), -1);
        clamp_offset();
    }

    void move_up()
    {
        selection = advance_shown(selection, -1);
        clamp_offset();
    }

    void move_down()
    {
        selection = advance_shown(selection, 1);
        clamp_offset();
    }

    void move_next()
    {
        if (selection == tid_detail->end())
            return;

        auto saved_selection = selection;

        while (selection != tid_detail->end()) {
            trace_detail_iter old_selection = selection;
            selection = advance_shown(selection, 1);

            if (selection == old_selection || selection == tid_detail->end()) {
                selection = saved_selection;
                break;
            }

            if (selection->indent == saved_selection->indent) {
                // Skip the return line if necessary
                if (selection->rec.fn == saved_selection->rec.fn &&
                        !selection->rec.call) {
                    selection = advance_shown(selection, 1);
                }
                break;
            }
        }
        clamp_offset();
    }

    void page_down()
    {
        trace_detail_iter cur_offset = tid_detail->begin() + offset;
        cur_offset = advance_shown(cur_offset, display_h);
        selection = advance_shown(selection, display_h);
        offset = cur_offset - tid_detail->begin();
        clamp_offset();
    }

    void page_up()
    {
        trace_detail_iter cur_offset = tid_detail->begin() + offset;
        cur_offset = advance_shown(cur_offset, -display_h);
        selection = advance_shown(selection, -display_h);
        offset = cur_offset - tid_detail->begin();
        clamp_offset();
    }

    void scroll_view(int distance)
    {
        trace_detail_iter it = tid_detail->begin() + offset;
        it = advance_shown(it, distance);
        offset = it - tid_detail->begin();
        clamp_offset();
    }

    void tree_collapse(bool clamp_ofs = true)
    {
        if (selection->rec.expanded && selection->rec.expandable) {
            selection->rec.expanded = false;
            int scan_indent = selection->indent;
            trace_detail_iter scan{selection};
            while (++scan != tid_detail->end()) {
                scan->rec.show = false;
                scan->rec.expanded = false;

                if (scan->indent == scan_indent)
                    break;
            }
        }

        if (clamp_ofs)
            clamp_offset();
    }

    void tree_expand()
    {
        if (!selection->rec.expanded && selection->rec.expandable) {
            selection->rec.expanded = true;

            int scan_indent = selection->indent;
            trace_detail_iter scan{selection};
            while (++scan != tid_detail->end()) {
                if (scan->indent == scan_indent + 1)
                    scan->rec.show = scan->rec.showable;

                if (scan->indent == scan_indent) {
                    scan->rec.show = scan->rec.showable;
                    break;
                }
            }

            clamp_offset();
        } else {
            move_down();
        }
    }

    void tree_expand_all()
    {
        selection->rec.expanded = true;

        int scan_indent = selection->indent;
        trace_detail_iter scan{selection};
        while (++scan != tid_detail->end()) {
            scan->rec.show = scan->rec.showable;
            scan->rec.expanded = scan->rec.expandable;

            if (scan->indent == scan_indent) {
                scan->rec.show = scan->rec.showable;
                break;
            }
        }
    }

    void move_caller()
    {
        int indent = selection->indent;

        do {
            selection = advance_shown(selection, -1);
        } while (selection != tid_detail->begin() &&
                 selection->indent >= indent);
        clamp_offset();
    }

    void elide_call()
    {
        auto saved_selection = selection;
        uint64_t fn = selection->rec.fn;
        for (auto it = tid_detail->begin(), en = tid_detail->end();
             it != en; ++it) {
            if (it->rec.fn == fn) {
                selection = it;
                tree_collapse(false);
            }
        }
        selection = saved_selection;
        clamp_offset();
    }

    void find_text()
    {
        echo();
        curs_set(TRUE);
        mvhline(display_h - 1, 0, ' ', display_w);
        mvprintw(display_h - 1, 0, ":");
        std::unique_ptr<char[]> buf(new char[display_w]);
        memset(buf.get(), 0, sizeof(buf[0]) * display_w);
        mvgetnstr(display_h - 1, 1, buf.get(), display_w - 1);
        noecho();
        curs_set(FALSE);

        search.reset(new std::regex(buf.get(), std::regex::optimize));

        find_next(1);
    }

    void find_next(int dir)
    {
        if (!search)
            return;

        auto scan = selection;

        for (;;) {
            if (dir > 0 && scan == tid_detail->end())
                return;

            if (dir < 0 && scan == tid_detail->begin())
                return;

            scan += dir;

            auto it = symbols->lower_bound(uint64_t(scan->rec.get_ip()));

            auto wtf = it->second.c_str();

            if (std::regex_search(it->second, *search)) {
                selection = scan;
                clamp_offset();
                break;
            }
        }
    }

    void handle_resize()
    {
        getmaxyx(stdscr, display_h, display_w);
        clamp_offset();
    }

    void handle_mouse()
    {
        MEVENT event;
        if (getmouse(&event) == OK) {
            if (event.bstate & BUTTON1_PRESSED) {
                selection = advance_shown(tid_detail->begin() + offset,
                                          event.y);
                clamp_offset();
            }
        }
    }

    void show_help()
    {

    }

    std::string filename;
    std::unique_ptr<sym_tab> symbols;
    trace_thread_map data;
    bool done;
    int tid;
    trace_detail_vector *tid_detail;
    trace_detail_iter selection;
    trace_detail_iter highlight_st;
    trace_detail_iter highlight_en;
    int64_t offset;
    int cursor_row;
    int display_h, display_w;
    std::unique_ptr<std::regex> search;
};

int main(int argc, char **argv)
{
    try {
        if (argc == 1)
            capture();
        else if (argc == 2) {
            std::unique_ptr<viewer>{new viewer(argv[1])}->run();
        }
    } catch (std::exception const& ex) {
        fprintf(stderr, "Error: %s\n", ex.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "Fatal error\n");
        return 1;
    }
    return 0;
}
