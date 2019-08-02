#include <spawn.h>

int posix_spawnp(pid_t *restrict pid,
                 char const *restrict path,
                 posix_spawn_file_actions_t const *fact,
                 posix_spawnattr_t const *restrict fatt,
                 char * const * restrict argv,
                 char *const * restrict envp)
{
    return -1;
}
