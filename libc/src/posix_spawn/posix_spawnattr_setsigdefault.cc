#include <spawn.h>

int posix_spawnattr_setsigdefault(
        posix_spawnattr_t *restrict satt,
        const sigset_t *restrict)
{
    return -1;
}
