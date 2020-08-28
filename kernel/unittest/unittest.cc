#include "unittest.h"
#include "kmodule.h"
#include "printk.h"
#include "vector.h"
#include "cxxstring.h"
#include "debug.h"

__thread int test_tls;
__thread int test_tls_2;
__thread int test_tls_3;

unittest::unit *unittest::unit::list_st;
unittest::unit *unittest::unit::list_en;

__BEGIN_NAMESPACE(unittest)

class test_filter {
public:
    test_filter();

    bool unknown_option(char const *option)
    {
        printdbg("Unknown option %s\n", option);
        return false;
    }

    bool init(int argc, char const * const *argv)
    {
        for (int i = 0; argc > 0 && i < int(size_t(argc)); ++i) {
            switch (argv[i][0]) {
            case '-':
                switch (argv[i][1]) {
                case '-':
                    // Long -- prefix options
                    break;

                    //
                    // Short options

                case 'i':
                    if (argc > i + 1)
                        permit.push_back(argv[i + 1]);
                    else
                        printdbg("Missing argument to test inclusion option\n");

                    break;

                case 'e':
                    if (argc > i + 1)
                        reject.push_back(argv[i + 1]);
                    else
                        printdbg("Missing argument to test inclusion option\n");

                    break;

                default:
                    return unknown_option(argv[i]);
                }
                break;

            }
        }
    }

    ext::vector<ext::string> permit;
    ext::vector<ext::string> reject;
};

__END_NAMESPACE

template<typename T>
class uniform_distribution_t {
public:
    uniform_distribution_t(T a, T b, T r)
        : a(a)
    {
        T d = b - a;

        // How many of the entire range fits in max
        T c = m / d;

        m = d * c;
    }

    bool operator()(T input, T& result) const
    {
        if (input < m) {
            // Good number
            result = (input % m) + a;
            return true;
        }

        // Provided random number is no good, try again
        return false;
    }

    T a;
    T m;
};

static int test_run_thread(void *)
{
    unittest::unit_ctx ctx;
    unittest::unit::run_all(&ctx);

    if (ctx.failure_count() == 0) {
        printdbg("All tests passed!\n");

    } else {
        printdbg("Test failures: %zu!\n", ctx.failure_count());
        return 1;
    }
    return 0;
}

int module_main(int argc, char const * const * argv)
{
#if 0
    test_run_thread(nullptr);
#else
    int tid = thread_create(nullptr,
                            test_run_thread, nullptr,
                            "unittest", 0, false, false);

    thread_close(tid);
#endif

    return 0;
}

void unittest::unit_ctx::fail(
        unit *test, char const *file, int line)
{
    (void)test;
    (void)file;
    (void)line;
    ++failures;
    cpu_debug_break();
}

void unittest::unit_ctx::skip(unittest::unit *test)
{
    printdbg("%s(%d): skipped %s\n",
             test->get_file(), test->get_line(), test->get_name());
    ++skipped;
}

unittest::unit::unit(const char *name, const char *test_file,
                     int test_line, bool init_enabled)
    : is_enabled(init_enabled)
    , name(name)
    , test_file(test_file)
    , test_line(test_line)
{
    // Link myself into the chain
    unit **prev_ptr = list_en ? &list_en->next : &list_st;
    *prev_ptr = this;
    list_en = this;
}

void unittest::unit::fail(char const *file, int line)
{
    printk("Test failed: %s %s(%d)\n", name, file, line);
    ctx->fail(this, file, line);
}

void unittest::unit::fail(const char *message, const char *file, int line)
{
    printk("Test failed: %s %s(%d): %s\n", name, file, line, message);
    ctx->fail(this, file, line);
}

void unittest::unit::set_ctx(unit_ctx *ctx)
{
    this->ctx = ctx;
}

template void unittest::unit::eq(int const&, int const&,
    const char *file, int line);
template void unittest::unit::eq(uint32_t const&, uint32_t const&,
    const char *file, int line);
template void unittest::unit::eq(bool const&, bool const&,
    const char *file, int line);
template void unittest::unit::eq(size_t const&, size_t const&,
    const char *file, int line);

const char *unittest::unit::get_name() const
{
    return name;
}

const char *unittest::unit::get_file() const
{
    return test_file;
}

int unittest::unit::get_line() const
{
    return test_line;
}

void unittest::unit::run_all(unit_ctx *ctx)
{
    for (unit *it = list_st; it; it = it->next) {
        if (unlikely(!it->enabled())) {
            ctx->skip(it);
            continue;
        }

        printk("%s: testing...\n", it->get_name());
        size_t prev_failures = ctx->failure_count();
        it->set_ctx(ctx);

        if (likely(!it->float_thread())) {
            it->invoke();
        } else {
            thread_t tid = thread_create(nullptr,
                    &unit::thread_fn, it,
                    "threaded_unit_test", 0, true, true);
            int cpu_nr = thread_current_cpu(tid);
            printdbg("Test thread cpu nr %d\n", cpu_nr);
            thread_wait(tid);
            thread_close(tid);
        }

        if (likely(prev_failures == ctx->failure_count())) {
            printk("%s: OK\n", it->get_name());
        } else {
            printk("*** FAILED! %s\n", it->get_name());
        }
    }
}

int unittest::unit::thread_fn(void *arg)
{
    unittest::unit *test = (unittest::unit *)arg;
    test->invoke();
    return 0;
}

void unittest::unit::eq_str(const char *expect,
    const char *value, const char *file, int line)
{
    if (unlikely(strcmp(expect, value))) {
        dbgout << name << " expected \"" << expect << '"' <<
        " but got " << value << '\n';
        fail(file, line);
    }
}
