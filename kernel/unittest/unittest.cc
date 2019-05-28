#include "unittest.h"
#include "printk.h"

_constructor(1000) void module_main()
{
    unittest::unit_ctx ctx;
    unittest::unit::run_all(&ctx);
}

void unittest::unit_ctx::fail(unittest::unit *test)
{
    ++failures;
}

unittest::unit::unit(const char *name)
    : name(name)
{
    // Link myself into the chain
    unit **prev_ptr = list_en ? &list_en->next : &list_st;
    *prev_ptr = this;
    list_en = this;
}

void unittest::unit::set_ctx(unittest::unit_ctx *ctx)
{
    this->ctx = ctx;
}

const char *unittest::unit::get_name() const
{
    return name;
}

void unittest::unit::run_all(unittest::unit_ctx *ctx)
{
    for (unit *it = list_st; it; it = it->next) {
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
