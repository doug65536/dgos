#pragma once
#include "sys_time.h"

_noreturn
void sys_exit(int exitcode);
long sys_futex(int *uaddr, int futex_op, int val,
               struct timespec const *timeout,
               int *uaddr2, int val3);
