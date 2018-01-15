#include <spawn.h>
#include "bits/posix_spawn_file_action.h"

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
    for (size_t i = 0; i < fa->__used; ++i)
        fa->__actions[i].destruct();

    free(fa->__actions);

    fa->__allocated = 0;
    fa->__used = 0;
    fa->__actions = nullptr;

    return 0;
}
