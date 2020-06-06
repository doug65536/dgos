#pragma once
#include "sys_time.h"
#include "process.h"

struct posix_spawn_file_actions_t;
struct posix_spawnattr_t;

_noreturn
void sys_exit(int exitcode);
long sys_futex(int *uaddr, int futex_op, int val,
               struct timespec const *timeout,
               int *uaddr2, int val3);
long sys_posix_spawn(pid_t *restrict pid,
                     char const *restrict path,
                     posix_spawn_file_actions_t const *file_actions,
                     posix_spawnattr_t const *restrict attr,
                     char const * * restrict argv,
                     char const * * restrict envp);

long sys_clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg);
