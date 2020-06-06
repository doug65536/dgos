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
    scp_t result = syscall6(uintptr_t(pid), uintptr_t(path),
                            uintptr_t(file_actions),
                            uintptr_t(attr),
                            uintptr_t(argv), uintptr_t(envp),
                            SYS_posix_spawn);

    if (likely(result >= 0))
        return 0;

    errno = errno_t(-result);
    return -1;
}
