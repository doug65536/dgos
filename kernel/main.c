
#include "cpu.h"
#include "mm.h"

int life_and_stuff = 42;

char buf[12];

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

int main()
{
    //init_cpu();

    short *screen = (short*)0xB8000;
    static char message[] = "In kernel!!!";
    for (int i = 0; message[i]; ++i)
        screen[i] = (short)(0x700 | message[i]);

    return 42;
}
