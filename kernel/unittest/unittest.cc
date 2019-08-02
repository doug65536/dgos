#include "unittest.h"
#include "kmodule.h"
#include "printk.h"
#include "vector.h"
#include "cxxstring.h"

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

    std::vector<std::string> permit;
    std::vector<std::string> reject;
};

__END_NAMESPACE

int module_main(int argc, char const * const * argv)
{
    unittest::unit_ctx ctx;
    unittest::unit::run_all(&ctx);

    if (ctx.failure_count() == 0) {
        printdbg("All tests passed!\n");

    } else {
        printdbg("FAILED!\n");
        return 1;
    }
    return 0;
}

void unittest::unit_ctx::fail(
        unit *test, char const *file, int line)
{
    (void)test;
    (void)file;
    (void)line;
    ++failures;
}

void unittest::unit_ctx::skip(unittest::unit *test)
{
    printdbg("%s(%d): skipped %s\n",
             test->get_file(), test->get_line(), test->get_name());
    ++skipped;
}

unittest::unit::unit(const char *name, const char *test_file, int test_line)
    : name(name)
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

void unittest::unit::set_ctx(unit_ctx *ctx)
{
    this->ctx = ctx;
}

void unittest::unit::eq(char const *expect, char *value,
                        const char *file, int line)
{
    if (strcmp(expect, value)) {
        dbgout << name << " expected \"" << expect <<
                  " but got " << value << "\n";
        fail(file, line);
    }
}

template void unittest::unit::eq(int&&, int&&,
    const char *file, int line);
template void unittest::unit::eq(uint32_t&&, uint32_t&&,
    const char *file, int line);
template void unittest::unit::eq(bool&&, bool&&,
    const char *file, int line);
template void unittest::unit::eq(size_t&&, size_t&&,
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
        if (!it->enabled()) {
            ctx->skip(it);
            continue;
        }

        printk("%s: testing...\n", it->get_name());
        size_t prev_failures = ctx->failure_count();
        it->set_ctx(ctx);
        it->invoke();
        if (prev_failures != ctx->failure_count()) {
            printk("*** FAILED! %s\n", it->get_name());
        } else {
            printk("%s: OK!\n", it->get_name());
        }
    }
}
