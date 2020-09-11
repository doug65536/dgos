#include <signal.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

__BEGIN_DECLS

__attribute__((__noreturn__))
void __signal_restorer(int sig, siginfo_t *info,
                       void (*restorer)(int, siginfo_t, void*));

__attribute__((__noreturn__, __visibility__("hidden")))
void __sigreturn(void *mctx)
{
    syscall1(scp_t(mctx), SYS_sigreturn);
    __builtin_unreachable();
}

__END_DECLS

int sigaction(int signum, struct sigaction const *act,
              struct sigaction *oldact)
{
    int status = syscall4(unsigned(signum), uintptr_t(act),
                          uintptr_t(oldact), scp_t(__signal_restorer),
                          SYS_sigaction);

    if (unlikely(status < 0)) {
        errno = -status;
        return -1;
    }

    return 0;
}
