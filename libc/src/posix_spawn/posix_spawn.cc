#include <spawn.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int posix_spawn(pid_t *restrict pid,
                char const *restrict path,
                posix_spawn_file_actions_t const *file_actions,
                posix_spawnattr_t const *restrict attr,
                char * const *restrict argv,
                char *const *restrict envp)
{
    return -1;
}
