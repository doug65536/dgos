#include <signal.h>
#include <errno.h>

void (*signal(int sig, void (*handler)(int)))(int)
{
//    sigaction act;
//    sigaction old_act;
//    act.sa_handler = handler;

//    // Cause the signal to become masked when its handler is running
//    sigemptyset(&act.sa_mask);
//    sigaddset(&act.sa_mask, sig);

//    sigaction(sig, &act, &old_act);

    errno = ENOSYS;

    return nullptr;
}
