#include <spawn.h>
//#include "bits/posix_spawn_file_action.h"

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fact)
{
    fact->__actions = nullptr;
    fact->__allocated = 0;
    fact->__used = 0;

    return 0;
}
