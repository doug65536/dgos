#pragma once

#include <sched.h>
#include <signal.h>
#include <sys/types.h>

// The <spawn.h> header shall define the posix_spawnattr_t and
// posix_spawn_file_actions_t types used in performing spawn operations.
typedef struct posix_spawn_file_actions_t posix_spawn_file_actions_t;
typedef struct posix_spawnattr_t posix_spawnattr_t;

//
// The <spawn.h> header shall define the flags that may be set in a
// posix_spawnattr_t object using the posix_spawnattr_setflags() function:

#define POSIX_SPAWN_RESETIDS        (1<<0)
#define POSIX_SPAWN_SETPGROUP       (1<<1)
#define POSIX_SPAWN_SETSCHEDPARAM   (1<<2)
#define POSIX_SPAWN_SETSCHEDULER    (1<<3)
#define POSIX_SPAWN_SETSIGDEF       (1<<4)
#define POSIX_SPAWN_SETSIGMASK      (1<<5)

// The following shall be declared as functions and may also be
// defined as macros. Function prototypes shall be provided.

int posix_spawn(pid_t *restrict pid,
                const char *restrict path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *restrict attr,
                char * const *restrict argv,
                char *const *restrict envp);

int posix_spawn_file_actions_addclose(
        posix_spawn_file_actions_t *fact, int fd);

int posix_spawn_file_actions_adddup2(
        posix_spawn_file_actions_t *fact,
        int from, int to);

int posix_spawn_file_actions_addopen(
        posix_spawn_file_actions_t *restrict fact,
        int fd, const char *restrict path, int, mode_t mode);

int posix_spawn_file_actions_destroy(
        posix_spawn_file_actions_t *fa);

int posix_spawn_file_actions_init(
        posix_spawn_file_actions_t *fact);

int posix_spawnattr_destroy(
        posix_spawnattr_t *fatt);

int posix_spawnattr_getsigdefault(
        const posix_spawnattr_t *restrict fatt,
        sigset_t *restrict set);

int posix_spawnattr_getflags(
        const posix_spawnattr_t *restrict fatt,
        short *restrict flags);

int posix_spawnattr_getpgroup(
        const posix_spawnattr_t *restrict fatt,
        pid_t *restrict pid);


int posix_spawnattr_getschedparam(
        const posix_spawnattr_t *restrict satt,
        struct sched_param *restrict);

int posix_spawnattr_getschedpolicy(
        const posix_spawnattr_t *restrict satt,
        int *restrict);


int posix_spawnattr_getsigmask(
        const posix_spawnattr_t *restrict satt,
        sigset_t *restrict);

int posix_spawnattr_init(posix_spawnattr_t *satt);

int posix_spawnattr_setsigdefault(
        posix_spawnattr_t *restrict satt,
        const sigset_t *restrict);

int posix_spawnattr_setflags(
        posix_spawnattr_t *satt,
        short flags);

int posix_spawnattr_setpgroup(
        posix_spawnattr_t *satt,
        pid_t pid);


int posix_spawnattr_setschedparam(
        posix_spawnattr_t *restrict satt,
        const struct sched_param *restrict param);

int posix_spawnattr_setschedpolicy(
        posix_spawnattr_t *satt,
        int policy);


int posix_spawnattr_setsigmask(
        posix_spawnattr_t *restrict satt,
        const sigset_t *restrict set);

int posix_spawnp(pid_t *restrict pid,
                 const char *restrict path,
                 const posix_spawn_file_actions_t *fact,
                 const posix_spawnattr_t *restrict fatt,
                 char * const * restrict argv,
                 char *const * restrict envp);

