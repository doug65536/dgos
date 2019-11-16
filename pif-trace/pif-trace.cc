//#include <ncurses.h>
#include <cstdio>
#include <cinttypes>
#include <map>
#include <set>
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
#include <cstdint>
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

static std::string to_eng(uint64_t n, bool seconds = false);

using sym_tab = std::multimap<uint64_t, std::string>;

using trace_item_vector = std::vector<trace_item>;

char symbol_file[] = "sym/kernel-tracing.sym";

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
        : out_file(nullptr)
        , stop(false)
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
                assert(write_size < INT_MAX);
                int wrote = gzwrite(out_file, buffer.data(), write_size);
                if (unlikely(wrote != int(write_size)))
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

sig_atomic_t volatile quit;

void sigint_handler(int)
{
    quit = true;
}

static constexpr uintptr_t canon_addr(uintptr_t addr)
{
    return uintptr_t(intptr_t(uintptr_t(addr) << 16) >> 16);
}

int capture(bool verbose = false)
{
    signal(SIGINT, sigint_handler);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, nullptr) < 0)
        throw trace_error("sigaction failed", errno);

    fprintf(stderr, "\nLoading symbols...");
    auto symbols = load_symbols();
    fprintf(stderr, "done\n");

    thread_tab threads;

    int notify = inotify_init1(IN_NONBLOCK);
    if (unlikely(notify < 0))
        throw trace_error("could not create inotify: %s\n", errno);

    int watch = inotify_add_watch(notify, ".", IN_CLOSE_WRITE);
    if (unlikely(watch < 0))
        throw trace_error("could not create inotify watch: %s\n", errno);

    int in_fd = open("dump/call-trace-out", O_EXCL);
    if (unlikely(in_fd < 0))
        throw trace_error("could not open call trace FIFO\n", errno);

    writer_thread_t writer;

    using clock = std::chrono::high_resolution_clock;
    clock::time_point last_sample = clock::now();
    size_t since_count = 0;

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
        int nevents = poll(polls, 2, 1000);

        // I don't really care whether poll failed if quitting
        if (quit)
            break;

        // Get error code or zero if no error
        int err = nevents < 0 ? errno : 0;

        if (nevents == 0) {
            printf(" Idle\r");
            fflush(stdout);
            continue;
        }

        if (unlikely(nevents < 0))
            throw trace_error("poll failed: ", err);

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

            since_count += got;
            auto now = clock::now();
            auto elap = now - last_sample;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        elap).count();
            if (ms >= 1000) {
                printf("  %s records/s\r",
                       to_eng(UINT64_C(1000) * since_count / ms).c_str());
                fflush(stdout);
                last_sample = now;
                since_count = 0;
            }

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

                if (unlikely(verbose)) {
                    int tid = item.get_tid();

                    // If the thread ID is zero, the CPU number is the thread
                    // ID because it must be so early to be the case
//                    if (!tid)
//                        tid = item.get_cid();

                    thread_state& thread = threads[tid];

                    if (!item.call)
                        --thread.indent;

                    // Lookup symbol from instruction pointer
                    auto it = symbols->lower_bound(canon_addr(item.fn));

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
            }

            if (unlikely(ofs && got_count))
                items[0] = items[got_count];
        }
    }

    return 0;
}

struct alignas(16) trace_detail {
    // The record from the trace log
    trace_record rec = {};

    // The index to the call that called this function
    size_t caller_index = 0;

    // The indent level at this point
    int16_t indent = 0;

    bool hot = false;

    bool operator<(uint64_t n) const
    {
        return rec.tsc < n;
    }

    static constexpr size_t unknown_caller = ~size_t(0);
};

//static_assert(sizeof(trace_detail) == 25, "Unexpected size");

using trace_detail_vector = std::vector<trace_detail>;
using trace_detail_iter = trace_detail_vector::iterator;
using trace_thread_map = std::map<int, trace_detail_vector>;

struct callpath_t {
    std::vector<uintptr_t> path;

    callpath_t() = default;
    callpath_t(callpath_t&&) = default;
    callpath_t(callpath_t const&) = default;
    callpath_t& operator=(callpath_t const&) = default;
    callpath_t& operator=(callpath_t&&) = default;

    template<typename C>
    callpath_t(trace_detail const* leaf, C const& collection)
    {
        do {
            path.push_back(uintptr_t{leaf->rec.get_ip()});
        } while (leaf->caller_index != leaf->unknown_caller &&
                 (leaf = &collection[leaf->caller_index]) != nullptr);

        std::reverse(std::begin(path), std::end(path));
    }

    bool operator<(callpath_t const& rhs) const noexcept
    {
        size_t i = 0;
        for (size_t e = std::min(path.size(), rhs.path.size());
             i != e; ++i) {
            bool is_lt = path[i] < rhs.path[i];
            bool is_gt = rhs.path[i] < path[i];

            // If equal, keep going to next level
            if (!is_lt & !is_gt)
                continue;

            return is_lt;
        }

        // If it reaches here, paths are equal up to end of shorter one
        // This one is less than the other one if this one is shorter
        return path.size() < rhs.path.size();
    }
};

struct callgraph_entry_t {
    uint64_t total_time;
    uint64_t child_time;
    uint64_t call_count;
};

using callpath_table_t = std::set<callpath_t>;

struct callgraph_compare_t {
    bool operator()(callpath_table_t::iterator lhs,
                    callpath_table_t::iterator rhs) const noexcept
    {
        return *lhs < *rhs;
    }
};

// Keyed on iterators into callpath table to deduplicate
using callgraph_t = std::map<
    callpath_table_t::iterator,
    callgraph_entry_t,
    callgraph_compare_t
>;

// Convert an extremely wide range of values to compact engineering format
static std::string to_eng(uint64_t n, bool seconds)
{
    std::string result;
    char const *unit;
    uint64_t divisor;
    uint64_t quot;
    int tenths;

    if (seconds) {
        static constexpr uint64_t clk_spd = 2500000000;

        // Translate cycle count to 1ps units
        auto ps = (__uint128_t{n} * 1000000000000U) / clk_spd;

        if (ps < 1000U) {
            // picoseconds
            unit = "p";
            divisor = 1;
        } else if (ps < 1000000U) {
            // nanoseconds
            unit = "n";
            divisor = 1000;
        } else if (ps < 1000000000U) {
            // microseconds
            //unit = "μ";
            unit = "u";
            divisor = 1000000;
        } else if (ps < 1000000000000U) {
            // milliseconds
            unit = "m";
            divisor = 1000000000;
        } else if (ps < 1000000000000000U) {
            // seconds
            unit = "s";
            divisor = 1000000000000;
        } else if (ps < 1000000000000000000U) {
            // kiloseconds
            unit = "k";
            divisor = 1000000000000000;
        } else if (ps < __uint128_t{1000000000000000000U} * 1000U) {
            // megaseconds
            unit = "M";
            divisor = 1000000000000000000U;
        } else {
            divisor = 0;
        }

        quot = divisor ? ps / divisor : 0;
        tenths = divisor ? int((ps * 10) / divisor) % 10 : 0;
    } else {
        if (n >= 1000000000000000000U) {
            unit = "E";
            divisor = 1000000000000000000U;
        } else if (n >= 1000000000000000U) {
            unit = "P";
            divisor = 1000000000000000U;
        } else if (n >= 1000000000000U) {
            unit = "T";
            divisor = 1000000000000U;
        } else if (n >= 1000000000U) {
            unit = "G";
            divisor = 1000000000U;
        } else if (n >= 1000000U) {
            unit = "M";
            divisor = 1000000U;
        } else if (n >= 1000U) {
            unit = "k";
            divisor = 1000U;
        } else {
            unit = " ";
            divisor = 1U;
        }

        quot = n / divisor;
        tenths = int((n * 10) / divisor) % 10;
    }

    if (likely(divisor)) {
        result = std::to_string(quot);

        if (result.length() < 2)
        {
            if (tenths) {
                result += '.';
                result += std::to_string(tenths);
            } else {
                result = "  " + result;
            }
        }

        while (result.length() < 3)
            result.insert(result.begin(), ' ');

        result += unit;
    }

    return result;
}

trace_thread_map load_trace(char const *filename, sym_tab const *syms)
{
    trace_thread_map thread_map;

    trace_item_vector buffer;

    static constexpr int buffer_count = 1048576 / sizeof(trace_item);
    static constexpr int buffer_size = buffer_count * sizeof(trace_item);

    gzFile file = gzopen64(filename, "r");

    if (!file) {
        int err = errno;
        throw trace_error(std::string("Unable to open ") + filename, err);
    }

    printf("Loading records...");
    fflush(stdout);

    size_t total_records = 0;

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

            ++total_records;

            int tid = item.get_tid();

            // If there is no thread id because too early, then use
            // cpu id as thread id, which is correct
            if (!tid)
                tid = item.get_cid();

            trace_detail_vector& detail_vector = thread_map[tid];

            trace_detail prev = !detail_vector.empty()
                    ? detail_vector.back()
                    : trace_detail{};

            int16_t indent;
            if (prev.rec.call && item.call)
                indent = prev.indent + 1;
            else if (!prev.rec.call && !item.call)
                indent = prev.indent - 1;
            else
                indent = prev.indent;

            detail_vector.push_back({ trace_record(item), ~size_t(0), indent });
        }
    }

    printf("%zu records\n", total_records);

    for (trace_thread_map::value_type& thread_data : thread_map) {
        trace_detail_vector& trace = thread_data.second;

        trace.shrink_to_fit();

        for (auto it = trace.begin(), en = trace.end(); it != en; ++it) {
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
                thread_data.first, trace.size());
    }

    printf("Sorting by timestamp\n");

    for (trace_thread_map::value_type& thread_data : thread_map) {
        trace_detail_vector& trace = thread_data.second;
        std::stable_sort(trace.begin(), trace.end(),
                         [&](trace_detail_vector::value_type const& lhs,
                         trace_detail_vector::value_type const& rhs) {
            return lhs.rec.tsc < rhs.rec.tsc;
        });
    }

    printf("Normalizing indents\n");

    for (trace_thread_map::value_type& thread_data : thread_map) {
        trace_detail_vector& trace = thread_data.second;

        int min_indent = !trace.empty() ? trace[0].indent : 0;

        for (size_t i = 0, e = trace.size(); i != e; ++i) {
            trace_detail& r = trace[i];

            min_indent = min_indent > r.indent ? r.indent : min_indent;
        }

        if (min_indent == 0)
            continue;

        for (size_t i = 0, e = trace.size(); i != e; ++i) {
            trace_detail& r = trace[i];

            r.indent -= min_indent;
        }
    }

    printf("Analyzing call stacks\n");

    using call_stack = std::vector<size_t>;

    call_stack calls;

    for (trace_thread_map::value_type& thread_data : thread_map) {
        trace_detail_vector& trace = thread_data.second;

        for (size_t i = 0, e = trace.size(); i != e; ++i) {
            trace_detail& r = trace[i];

            // Annotate every entry with its caller
            r.caller_index = !calls.empty() ? calls.back() : r.unknown_caller;

            // Update global call stack
            if (r.rec.call) {
                calls.push_back(i);
            } else {
                if (unlikely(calls.empty())) {
                    fprintf(stderr, "Discarded return with no call\n");
                    continue;
                }

                auto caller = calls.back();
                calls.pop_back();

                // Scan back over range and find hottest path
                // (here, where we conveniently know where the caller
                // entered this function and we just exited it
                size_t hottest_index = e;
                size_t hottest_time = 0;
                for (size_t k = caller + 1; k < i; ++k) {
                    if (trace[caller].indent == trace[k].indent - 1) {
                        if (hottest_time < trace[k].rec.total_time) {
                            hottest_time = trace[k].rec.total_time;
                            hottest_index = k;
                        }
                    }
                }

                if (hottest_index != e)
                    trace[hottest_index].hot = true;

                trace_detail& entry_ent = trace[caller];

                auto elapsed = r.rec.tsc - entry_ent.rec.tsc;

                entry_ent.rec.total_time += elapsed;

                // Add elapsed time to child time of caller
                if (likely(!calls.empty())) {
                    trace_detail& caller_ent = trace[calls.back()];
                    caller_ent.rec.child_time += elapsed;
                }
            }
        }

        calls.clear();
    }

    // A pair that holds total time and self time
    struct flat_profile_stats_t {
        uint64_t total_time;
        uint64_t self_time;
        uint64_t calls;

        flat_profile_stats_t()
            : total_time{}
            , self_time{}
            , calls{}
        {
        }

        flat_profile_stats_t(flat_profile_stats_t const&) = default;
        flat_profile_stats_t& operator=(flat_profile_stats_t const&) = default;
    };

    // A map keyed by function address with a total pair as value
    using flat_profile_map_t = std::map<uint64_t, flat_profile_stats_t>;

    // A pair with fn address as first, an embedded total/self pair as second
    struct flat_profile_triple_t {
        uint64_t fn;
        flat_profile_stats_t stats;

        flat_profile_triple_t()
            : fn{}
            , stats{}
        {
        }

        flat_profile_triple_t(uint64_t fn, flat_profile_stats_t stats)
            : fn{fn}
            , stats{stats}
        {
        }

        flat_profile_triple_t(flat_profile_triple_t const&) = default;
        flat_profile_triple_t& operator=(
                    flat_profile_triple_t const&) = default;
    };

    flat_profile_map_t flat_profile;

    // Run flat profile analysis
    for (trace_thread_map::value_type& thread_data : thread_map) {
        trace_detail_vector& trace = thread_data.second;

        for (size_t i = 0, e = trace.size(); i != e; ++i) {
            trace_detail& r = trace[i];

            if (r.rec.call) {
                auto& entry = flat_profile[canon_addr(r.rec.fn)];

                // Total time and self time
                entry.total_time += r.rec.total_time;
                entry.self_time += r.rec.total_time - r.rec.child_time;
                ++entry.calls;
            }
        }
    }

    std::vector<flat_profile_triple_t> sorted_flat_profile;

    uint64_t (flat_profile_stats_t::*time_member) =
            &flat_profile_stats_t::self_time;

    for (auto& i : flat_profile)
        sorted_flat_profile.push_back({ i.first, i.second });

    std::sort(std::begin(sorted_flat_profile), std::end(sorted_flat_profile),
              [&](flat_profile_triple_t const& lhs,
              flat_profile_triple_t const& rhs) {
        // > for descending sort
        // < for descending sort
        return lhs.stats.*time_member < rhs.stats.*time_member;
    });

    printf("calls elap function\n");
    printf("----- ---- --------\n");
    for (auto& i : sorted_flat_profile) {
        auto it = syms->lower_bound(canon_addr(i.fn));
        printf("%s  %s  %s\n",
               to_eng(i.stats.calls).c_str(),
               to_eng(i.stats.*time_member, true).c_str(),
               it->second.c_str());
    }

    //exit(0);

//    printf("Deduplicating call paths\n");

//    callpath_table_t call_paths;

//    for (trace_thread_map::value_type& tid_pair : thread_map) {
//        trace_detail_vector& trace = tid_pair.second;

//        for (size_t i = 0, e = trace.size(); i != e; ++i) {
//            trace_detail& r = trace[i];

//            call_paths.insert(callpath_t{&r, trace});
//        }
//    }

//    printf("Building call graph statistics\n");

//    callgraph_t call_graph;

//    for (trace_thread_map::value_type& tid_pair : thread_map) {
//        trace_detail_vector& trace = tid_pair.second;

//        for (size_t i = 0, e = trace.size(); i != e; ++i) {
//            trace_detail& r = trace[i];

//            callgraph_entry_t& ent = call_graph[
//                    call_paths.find(callpath_t{&r, trace})];

//            ent.call_count += r.rec.call;
//            ent.total_time += r.rec.total_time;
//            ent.child_time += r.rec.child_time;
//        }
//    }

//    printf("Sorting hot call stacks\n");

//    std::vector<std::pair<
//            std::remove_const<callgraph_t::value_type::first_type>::type,
//            callgraph_t::value_type::second_type>> sorted_call_graph{
//        std::begin(call_graph), std::end(call_graph)};

//    std::sort(std::begin(sorted_call_graph), std::end(sorted_call_graph),
//              [&](auto const& lhs, auto const& rhs) {
//        return lhs.second.total_time > rhs.second.total_time;
//    });

    return thread_map;
}

static bool is_detail_shown(trace_detail const& item)
{
    return item.rec.show;
}

// Include down here because of ridiculous
// #define namespace pollution
#define _XOPEN_SOURCE_EXTENDED 1
#include <ncurses.h>

class viewer {
public:
    viewer(char const *filename)
        : tid_detail(nullptr)
        , symbols(load_symbols())
        , filename(filename)
        , data(load_trace(filename, symbols.get()))
        , offset(0)
        , tid_index(0)
        , tid(0)
        , cursor_row(0)
        , done(false)
    {
        tid_detail = &data[tid];
        tid_list.reserve(data.size());
        for (trace_thread_map::value_type &pair : data)
            tid_list.push_back(pair.first);
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

        max_offset = (!tid_detail->empty()
                ? advance_shown(tid_detail->end(), -display_h)
                : tid_detail->begin()) - tid_detail->begin();

        trace_detail_iter item = update_selection();

        update_highlight_range();

        // Title
        init_pair(1, COLOR_BLACK, COLOR_WHITE);

        // Non-highlighted
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_YELLOW, COLOR_BLUE);

        // Highlight of selected function
        init_pair(2, COLOR_WHITE, COLOR_BLACK);
        init_pair(3, COLOR_WHITE, COLOR_BLUE);

        int color_pair = 1;

        attron(A_STANDOUT);
        attron(A_BOLD);

        attron(COLOR_PAIR(color_pair));
        mvprintw(0, 0, "%*s", display_w, "");
        mvprintw(0, 0, "CID");
        mvprintw(0, 4, "TID");
        mvprintw(0, 8, "I");
        mvprintw(0, 10, "Self/Totl");
        mvprintw(0, 20, "Function");
        attroff(COLOR_PAIR(color_pair));

        attroff(A_BOLD);

        int y;
        for (y = 0; y < display_h - 1 && item != tid_detail->end();
             ++y, ++item) {
            // Find visible item
            item = std::find_if(item, tid_detail->end(), is_detail_shown);

            if (item == tid_detail->end())
                break;

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

            color_pair = 4;

            if (item >= highlight_st && item <= highlight_en)
                color_pair = 2;

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

            auto&& eng_total_time = rec.call
                    ? to_eng(rec.total_time, true) : std::string("    ");
            auto&& eng_self_time = rec.call
                    ? to_eng(rec.total_time - rec.child_time, true)
                    : eng_total_time;

            if (item->hot)
                attron(A_BOLD);
            else
                attroff(A_BOLD);

            attron(COLOR_PAIR(color_pair));
            mvprintw(y + 1, 0, "%3d", rec.get_cid());
            attroff(COLOR_PAIR(color_pair));

            attron(COLOR_PAIR(color_pair+1));
            mvprintw(y + 1, 4, "%3d", tid);
            attroff(COLOR_PAIR(color_pair+1));

            attron(COLOR_PAIR(color_pair));
            mvprintw(y + 1, 8, "%c", rec.irq_en ? 'e' : 'd');
            attroff(COLOR_PAIR(color_pair));

            attron(COLOR_PAIR(color_pair+1));
            mvprintw(y + 1, 10, "%s%c%s",
                     eng_self_time.c_str(),
                     rec.call ? '/' : ' ',
                     eng_total_time.c_str());
            attroff(COLOR_PAIR(color_pair+1));

            attron(COLOR_PAIR(color_pair));
            mvprintw(y + 1, 20, "%*s %s ",
                     item->indent * 2, "",
                     tree_widget);

            mvprintw(y + 1, 20 + item->indent * 2 + 4, " %*s",
                     -display_w + 16 + item->indent * 2 + 4,
                     it->second.c_str());
            attroff(COLOR_PAIR(color_pair));

            if (color_pair != 2)
                attron(COLOR_PAIR(color_pair));

            mvprintw(y + 1, 3, " ");
            mvprintw(y + 1, 7, " ");
            mvprintw(y + 1, 9, " ");
            mvprintw(y + 1, 19, " ");

            if (color_pair != 2)
                attroff(COLOR_PAIR(color_pair));
        }

        while (++y < display_h)
            mvprintw(y, 0, "%*s", display_w, "XXXXXXXXX");

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
        offset = std::min(max_offset, offset);
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
        int key = wgetch(stdscr);

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
        case KEY_F(11):
            return (tree_expand(), move_down());

        case KEY_DOWN:
            return move_down();

        case '-':   // fall through
        case KEY_LEFT:
            return tree_left();

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

        case 'n':   // fall through
        case KEY_F(3):
            return find_next(1);

        case 'N':
            return find_next(-1);

        case '<':
            return prev_thread();

        case '>':
            return next_thread();

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
        int dir = (distance >= 0 ? 1 : -1);

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

            if (it != limit && is_detail_shown(*it)) {
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
        if (st > en)
            std::swap(st, en);
        for (count = 0; st != en; ++st)
            count += is_detail_shown(*st);

        return count;
    }

    void clamp_offset()
    {
        auto frac7_8th = ((7 * display_h) >> 3);

        // Go back from the selection 7/8ths of the screen height
        auto min_offset = advance_shown(selection, -frac7_8th) -
                tid_detail->begin();

        // Go forward from the selection 7/8ths of the screen height
        auto max_offset = advance_shown(selection, frac7_8th - display_h + 2) -
                tid_detail->begin();

        if (offset > max_offset)
            offset = max_offset;
        if (offset < min_offset)
            offset = min_offset;

        trace_detail_iter cur_offset = tid_detail->begin() + offset;
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
            auto last_shown = scan;
            while (++scan != tid_detail->end()) {
                if (scan->indent == scan_indent + 1)
                    scan->rec.show = scan->rec.showable;

                if (scan->indent == scan_indent) {
                    scan->rec.show = scan->rec.showable;
                    break;
                }

                // If it is a return and the last shown thing was the same
                // indent and function
                if (!last_shown->rec.expanded &&
                        !scan->rec.call &&
                        last_shown->rec.fn == scan->rec.fn)
                    scan->rec.show = false;

                if (scan->rec.show)
                    last_shown = scan;
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

    void tree_left()
    {
        if (!selection->rec.call) {
            // The selection is a return, go up to call entry
            trace_detail_iter scan{selection};

            while (scan != tid_detail->begin()) {
                --scan;
                if (scan->indent == selection->indent)
                    break;
            }

            selection = scan;

            clamp_offset();
        } else if (selection->rec.call && selection->rec.expanded) {
            // The selection is an expanded call
            return tree_collapse();
        } else if (selection->rec.call &&
                   !selection->rec.expanded &&
                   selection != tid_detail->begin()) {
            // The selection is a collapsed call
            trace_detail_iter scan{selection};

            while (scan != tid_detail->begin()) {
                --scan;
                if (scan->rec.show)
                    break;
            }

            selection = scan;
            clamp_offset();
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

            if (std::regex_search(it->second, *search)) {
                selection = scan;
                clamp_offset();
                break;
            }
        }
    }

    void prev_thread()
    {
        uint64_t selected_tsc = selection->rec.tsc;

        if (tid_index > 0) {
            tid = tid_list[--tid_index];
            tid_detail = &data[tid];
        }

        selection = std::lower_bound(tid_detail->begin(), tid_detail->end(),
                                     selected_tsc);
        clamp_offset();
    }

    void next_thread()
    {
        uint64_t selected_tsc = selection->rec.tsc;

        if (tid_index + 1 < tid_list.size()) {
            tid = tid_list[++tid_index];
            tid_detail = &data[tid];
        }

        selection = std::lower_bound(tid_detail->begin(), tid_detail->end(),
                                     selected_tsc);
        clamp_offset();
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
        static constexpr char const * const help_strings[] = {
            "e        collapse selected call everywhere",
            "u        move up to caller",
            "<F10>    step over",
            "<F11>    step into",
            "",
            "<up>     Previous line",
            "<down>   Next line",
            "<left>   Collapse call",
            "<right>  Expand call",
            "<pgup>   Move selection down one page",
            "<pgdn>   Move selection up one page",
            "",

            "/        find regex",
            "n        find next regex match",
            "N        find previous regex match",
            "",
            "[        Scroll up",
            "]        Scroll down",
            "t <home> go to top",
            "b <end>  go to bottom",
            "*        expand all",
            "",
            "<        previous thread",
            ">        next thread"
            "",
            "q        quit"
        };

        static constexpr size_t help_string_count =
                sizeof(help_strings)/sizeof(*help_strings);

        static constexpr size_t help_string_count_div2 =
                help_string_count / 2;

        size_t max_len = 0;
        for (auto s : help_strings)
            max_len = std::max(max_len, strlen(s));

        int width = (max_len + 4) * 2 + 2;
        int height = (help_string_count + 4) / 2 + 3;

        WINDOW *win = newwin(height, width,
                             (display_h - height) / 2,
                             (display_w - width) / 2);
        //box(win, 0x95, u'•');
        touchwin(win);
        for (size_t i = 0; i < help_string_count; ++i)
            mvwprintw(win,
                      i < help_string_count_div2
                      ? i + 2 : (i - help_string_count_div2 + 2),
                      i < help_string_count_div2
                      ? 2 : (max_len + 4),
                      help_strings[i]);
        wrefresh(win);

        getchar();

        endwin();
    }

    trace_detail_vector *tid_detail;
    std::vector<int> tid_list;
    std::unique_ptr<std::regex> search;
    std::unique_ptr<sym_tab> symbols;
    std::string filename;
    trace_thread_map data;
    trace_detail_iter selection;
    trace_detail_iter highlight_st;
    trace_detail_iter highlight_en;
    int64_t offset;
    int64_t max_offset;
    size_t tid_index;
    int tid;
    int cursor_row;
    int display_h, display_w;
    bool done;
};

int profile_trace(char const *filename)
{
    std::unique_ptr<sym_tab> symbols = load_symbols();
    trace_thread_map dump = load_trace(filename, symbols.get());

    /// The call trace is analyzed to find the following:
    ///  - call count per function
    ///  - elapsed time per call
    ///
    /// A call stack vector is maintained, the function address is pushed on
    /// every call entry and popped on every exit.
    ///
    /// A call stack per function is maintained. Each time a function is
    /// entered, the trace detail index is pushed to the function's stack.
    /// At function exit, the stack is consulted to find the entry tsc
    /// and calculate elapsed time and update
    ///
    /// Each
    ///
    ///  - self time per call
    ///  - total time per call
    ///
    return 0;
}

int dump_trace(char const *filename)
{
    std::unique_ptr<sym_tab> symbols = load_symbols();
    trace_thread_map dump = load_trace(filename, symbols.get());

    using record = std::pair<trace_detail_vector::value_type const&, uint16_t>;
    using linear_map_t = std::map<uint64_t, record>;
    linear_map_t linear_map;

    for (trace_thread_map::value_type const& tid_data : dump) {
        for (trace_detail_vector::value_type const& item : tid_data.second) {
            linear_map.emplace_hint(
                        linear_map.end(), item.rec.tsc,
                        record(item, tid_data.first));
        }
    }

    for (linear_map_t::value_type const& item_pair : linear_map) {
        record const& item = item_pair.second;
        uint64_t ip = uint64_t(item.first.rec.get_ip());
        auto it = symbols->lower_bound(ip);

        char const *tree_widget;
        if (item.first.rec.expandable) {
            if (item.first.rec.expanded)
                tree_widget = "->";
            else
                tree_widget = "+>";
        } else {
            if (item.first.rec.call)
                tree_widget = " >";
            else
                tree_widget = " <";
        }

        printf("(c: %3d, t: %3d, I: %d) %*s %s %s\n",
               item.first.rec.get_cid(), item.second,
               item.first.rec.irq_en,
               item.first.indent * 2, "",
               tree_widget,
               it->second.c_str());
    }

    return 0;
}

int main(int argc, char **argv)
{
    //setlocale(LC_ALL, "");
    //setlocale( LC_CTYPE, "" );
    //setlocale(LC_ALL, "C.UTF-8");

    try {
        if (argc == 1)
            capture();
        else if (argc == 2) {
            std::unique_ptr<viewer>{new viewer(argv[1])}->run();
        } else if (argc > 2) {
            if (!strcmp(argv[1], "--dump")) {
                return dump_trace(argv[2]);
            } else if (!strcmp(argv[1], "--profile")) {
                return profile_trace(argv[2]);
            }
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
