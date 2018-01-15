#include <spawn.h>

int posix_spawnattr_getsigmask(
        const posix_spawnattr_t *restrict satt,
        sigset_t *restrict)
{
    return -1;
}
