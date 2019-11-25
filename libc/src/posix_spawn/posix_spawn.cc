#include <sys/likely.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int posix_spawn(pid_t *restrict pid,
                char const *restrict path,
                posix_spawn_file_actions_t const *file_actions,
                posix_spawnattr_t const *restrict attr,
                char const * * restrict argv,
                char const * * restrict envp)
{
    long result = syscall6(long(pid), long(path), long(file_actions),
                           long(attr), long(argv), long(envp),
                           SYS_posix_spawn);

    if (likely(result >= 0))
        return 0;

    errno = errno_t(-result);
    return -1;
}
