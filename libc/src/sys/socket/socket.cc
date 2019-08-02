#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int socket(int __domain, int __type, int __protocol)
{
     int status = syscall3(__domain, __type, __protocol, SYS_socket);

     if (status >= 0)
         return status;

     errno = -status;

     return -1;
}

