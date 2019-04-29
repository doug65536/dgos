#include <spawn.h>

int posix_spawnattr_getsigmask(
        posix_spawnattr_t const *restrict satt,
        sigset_t *restrict)
{
    return -1;
}
