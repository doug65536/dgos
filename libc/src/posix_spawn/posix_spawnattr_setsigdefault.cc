#include <spawn.h>

int posix_spawnattr_setsigdefault(
        posix_spawnattr_t *restrict satt,
        sigset_t const *restrict)
{
    return -1;
}
