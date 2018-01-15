#include <spawn.h>

int posix_spawnattr_getsigdefault(
        const posix_spawnattr_t *restrict fatt,
        sigset_t *restrict set)
{
    return -1;
}
