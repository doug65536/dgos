#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

#define AF_INET 2

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

#define IPPROTO_IP  0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

typedef uint32_t socklen_t;
typedef uint32_t sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

int socket(int __domain, int __type, int __protocol);
int send(int __fd, void const *__buf, int __len, int __flags);
int recv(int __fd, void *__buf, int __len, int __flags);

int accept(int __fd, struct sockaddr *__addr, socklen_t *__addrlen);
int bind(int __fd, struct sockaddr *addr, socklen_t addrlen);

__END_DECLS
