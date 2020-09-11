#include "sys_signal.h"
#include "process.h"
#include "user_mem.h"
#include "thread.h"

int sys_sigaction(int signum,
                  struct sigaction const *user_act,
                  struct sigaction *user_oldact,
                  void *restorer)
{
    if (unlikely(signum < 0 || signum > 31))
        return -int(errno_t::EINVAL);

    process_t *process = fast_cur_process();

    struct sigaction act;

    if (likely(process->use64)) {
        // Copy user sigaction
        if (unlikely(!mm_copy_user(&act, user_act, sizeof(act))))
            return -int(errno_t::EFAULT);
    } else {
        struct sigaction32 act32;

        // Copy user sigaction (32-bit)
        if (unlikely(!mm_copy_user(&act32, user_act, sizeof(act32))))
            return -int(errno_t::EFAULT);

        // Convert fields with normal instructions,
        // not memory manipulations
        act.sa_flags = act32.sa_flags;
        act.sa_handler = (void(*)(int))uintptr_t(act32.sa_handler);
        act.sa_mask = act32.sa_mask;
        act.sa_sigaction = (void(*)(int, siginfo_t *, void *))
                (void*)(uintptr_t(act32.sa_sigaction));
        act.sa_restorer = (void(*)(int, siginfo_t *, void (*handler)
                                   (int, siginfo_t*, void*)))restorer;
    }

    process->sigrestorer = (__sig_restorer_t)restorer;
    process->sighand[signum].sa_sigaction = act.sa_sigaction;
    process->sighand[signum].sa_handler = act.sa_handler;
    process->sighand[signum].sa_mask = act.sa_mask;
    process->sighand[signum].sa_flags = act.sa_flags;
    process->sighand[signum].sa_restorer = act.sa_restorer;

    return 0;
}
