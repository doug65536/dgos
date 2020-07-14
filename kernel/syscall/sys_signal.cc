#include "sys_signal.h"
#include "process.h"
#include "user_mem.h"

int sys_sigaction(int signum,
                  struct sigaction const *user_act,
                  struct sigaction *user_oldact)
{
    process_t *process = fast_cur_process();

    struct sigaction act;

    if (process->use64) {
        if (unlikely(!mm_copy_user(&act, user_act, sizeof(act))))
            return -int(errno_t::EFAULT);
    } else {
        struct sigaction32 act32;

        if (unlikely(!mm_copy_user(&act32, user_act, sizeof(act32))))
            return -int(errno_t::EFAULT);

        act.sa_flags = act32.sa_flags;
        act.sa_handler = (void(*)(int))uintptr_t(act32.sa_handler);
        act.sa_mask = act32.sa_mask;
        act.sa_sigaction = (void(*)(int, siginfo_t *, void *))
                uintptr_t(act32.sa_sigaction);
    }

    return -int(errno_t::ENOSYS);
}
