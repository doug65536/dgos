
#include "cpu.h"
#include "mm.h"
#include "printk.h"

int life_and_stuff = 42;

char buf[12];

extern void (*device_constructor_list[])(void);

__thread int tls_thing;
//__thread int tls_thing_2;
__thread int tls_initialized_thing = 42;
__thread int tls_initialized_thing_2 = 43;
//__thread char tls_buf[10] = {24};

volatile void *trick;

static __attribute__((constructor)) void start_me_up()
{
    tls_thing = 1;
    //tls_thing_2 = 23;
    tls_initialized_thing = 2;
    tls_initialized_thing_2 = 3;
    trick = &tls_initialized_thing;
}

static __attribute__((destructor)) void goin_down()
{
    //tls_thing = -22;
}

// Pull in the device constructors
// to cause them to be initialized
void (** volatile device_list)(void) = device_constructor_list;

#define TEST_FORMAT(f, t, v) \
    printk("Come on %10s -> '" f \
    "' 99=%d\n", f, (t)v, 99)

int main()
{
    printk("Testing!\n");
    printk("Testing again!\n");
    for (int i = 0; i < 22; ++i)
        printk("... and again!\n");

    TEST_FORMAT("%hhd", signed char, 42);
    TEST_FORMAT("%hd", short, 42);
    TEST_FORMAT("%d", int, 42);
    TEST_FORMAT("%ld", long, 42);
    TEST_FORMAT("%lld", long long, 42);
    TEST_FORMAT("%jd", intmax_t, 42);
    TEST_FORMAT("%zd", ssize_t, 42);
    TEST_FORMAT("%td", ptrdiff_t, 42);

    TEST_FORMAT("%5hhd", signed char, 42);
    TEST_FORMAT("%5hd", short, 42);
    TEST_FORMAT("%5d", int, 42);
    TEST_FORMAT("%5ld", long, 42);
    TEST_FORMAT("%5lld", long long, 42);
    TEST_FORMAT("%5jd", intmax_t, 42);
    TEST_FORMAT("%5zd", ssize_t, 42);
    TEST_FORMAT("%5td", ptrdiff_t, 42);

    TEST_FORMAT("%5.4hhd", signed char, 42);
    TEST_FORMAT("%5.4hd", short, 42);
    TEST_FORMAT("%5.4d", int, 42);
    TEST_FORMAT("%5.4ld", long, 42);
    TEST_FORMAT("%5.4lld", long long, 42);
    TEST_FORMAT("%5.4jd", intmax_t, 42);
    TEST_FORMAT("%5.4zd", ssize_t, 42);
    TEST_FORMAT("%5.4td", ptrdiff_t, 42);

    return 42;
}
