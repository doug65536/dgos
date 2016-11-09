
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

int main()
{
    printk("Testing!\n");
    printk("Testing again!\n");
    printk("... and again!\n");
    printk("Come on -> %d\n", 42);

    return 42;
}
