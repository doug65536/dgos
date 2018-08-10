#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

#define AF_INET 2

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

#define IPPROTO_IP  0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

int socket(int __domain, int __type, int __protocol);
int send(int __fd, void const *__buf, int __len, int __flags);
int recv(int __fd, void *__buf, int __len, int __flags);

__END_DECLS
