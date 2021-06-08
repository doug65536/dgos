#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

__thread char ttyname_buffer[TTY_NAME_MAX];

char *ttyname_r(int fd)
{
    if (ttyname_r(fd, ttyname_buffer, sizeof(ttyname_buffer)))
        return ttyname_buffer;
    return nullptr;
}
