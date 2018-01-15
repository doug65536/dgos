#include <spawn.h>

int posix_spawn_file_actions_addopen(
        posix_spawn_file_actions_t *restrict fact,
        int fd, const char *restrict path, int oflag, mode_t mode)
{
    return -1;
}

