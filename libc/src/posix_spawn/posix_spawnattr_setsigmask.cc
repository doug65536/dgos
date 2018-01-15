#include <spawn.h>

int posix_spawnattr_setsigmask(
        posix_spawnattr_t *restrict satt,
        const sigset_t *restrict set)
{
    return -1;
}
