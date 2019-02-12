#include "sys_process.h"
#include "process.h"
#include "printk.h"

void sys_exit(int exitcode)
{
    process_t::exit(-1, exitcode);
}


#define FUTEX_PRIVATE_FLAG 0x80000000
#define FUTEX_

void sys_futex(int *uaddr, int futex_op, int val,
               struct timespec const *timeout, int *uaddr2, int val3)
{
    switch (futex_op) {

    }
}
