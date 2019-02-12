#pragma once

void sys_exit(int exitcode);
void sys_futex(int *uaddr, int futex_op, int val,
               struct timespec const *timeout,
               int *uaddr2, int val3);