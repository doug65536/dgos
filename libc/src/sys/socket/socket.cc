#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int socket(int __domain, int __type, int __protocol)
{
     int status = syscall3(unsigned(__domain), unsigned(__type),
                           unsigned(__protocol), SYS_socket);

     if (likely(status >= 0))
         return status;

     errno = -status;

     return -1;
}

