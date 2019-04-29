#include <spawn.h>

int posix_spawnattr_getschedparam(
        posix_spawnattr_t const *restrict satt,
        struct sched_param *restrict)
{
    return -1;
}
