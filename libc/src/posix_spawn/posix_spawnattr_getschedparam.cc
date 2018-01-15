#include <spawn.h>

int posix_spawnattr_getschedparam(
        const posix_spawnattr_t *restrict satt,
        struct sched_param *restrict)
{
    return -1;
}
