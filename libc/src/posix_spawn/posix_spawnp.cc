#include <spawn.h>

int posix_spawnp(pid_t *restrict pid,
                 const char *restrict path,
                 const posix_spawn_file_actions_t *fact,
                 const posix_spawnattr_t *restrict fatt,
                 char * const * restrict argv,
                 char *const * restrict envp)
{
    return -1;
}
