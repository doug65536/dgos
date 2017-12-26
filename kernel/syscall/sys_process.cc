#include "sys_process.h"
#include "process.h"
#include "printk.h"

void sys_exit(int exitcode)
{
    //process_t::exit(-1, exitcode);
    printdbg("syscall bench %d cycles\n",
             exitcode / 8000000);
}
