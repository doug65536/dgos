#include <spawn.h>

int posix_spawnattr_setschedparam(
        posix_spawnattr_t *restrict satt,
        const struct sched_param *restrict param)
{
    return -1;
}
