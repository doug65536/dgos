#include "sys_exit.h"
#include "process.h"

int sys_exit(int error_code)
{
    process_t *p = thread_current_process();
    process_t::exit(p->pid, error_code);
}