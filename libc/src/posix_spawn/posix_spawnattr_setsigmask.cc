#include <spawn.h>

int posix_spawnattr_setsigmask(
        posix_spawnattr_t *restrict satt,
        sigset_t const *restrict set)
{
    return -1;
}
