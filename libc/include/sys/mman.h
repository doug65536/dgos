#pragma once
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

__BEGIN_DECLS

#define PROT_NONE           0x00
#define PROT_READ           0x01
#define PROT_WRITE          0x02
#define PROT_EXEC           0x04

#define MAP_FIXED           0x00000001

#define MAP_LOCKED          0x00000002
#define MAP_STACK           0x00000004
#define MAP_GROWSDOWN       0x00000008
#define MAP_HUGETLB         0x00000010
#define MAP_NORESERVE       0x00000020
#define MAP_POPULATE        0x00000040
#define MAP_UNINITIALIZED   0x00000080
#define MAP_32BIT           0x00000100
#define MAP_ANONYMOUS       0x00000200
#define MAP_INVALID_MASK    0x003FFC00
#define MAP_USER_MASK       0x000003FF

/// Ignored. Redundant.
#define MAP_DENYWRITE       0
#define MAP_EXECUTABLE      0
#define MAP_FILE            0
#define MAP_NONBLOCK        0

void *mmap(void *addr, size_t length, int prot,
           int flags, int fd, off_t offset);

int munmap(void *__addr,
        size_t size);

#define MREMAP_MAYMOVE      0x00000002
#define MREMAP_FIXED        0x00000001
#define MREMAP_INVALID_MASK 0xFFFFFFFC

void *mremap(void *__old_address,
        size_t __old_size,
        size_t __new_size,
        int __flags,
        void *__new_address = nullptr);

int mprotect(void *__addr, size_t __len, int __prot);

#define MADV_NORMAL         (0)
#define MADV_SEQUENTIAL     (1)
#define MADV_RANDOM         (-1)
#define MADV_WILLNEED       (2)
#define MADV_DONTNEED       (-2)
#define MADV_REMOVE         (7)
#define MADV_DOFORK         (3)
#define MADV_DONTFORK       (-3)
#define MADV_HWPOISON       (8)
#define MADV_SOFT_OFFLINE   (-8)
#define MADV_MERGEABLE      (4)
#define MADV_UNMERGEABLE    (-4)
#define MADV_HUGEPAGE       (5)
#define MADV_NOHUGEPAGE     (-5)
#define MADV_DODUMP         (6)
#define MADV_DONTDUMP       (-6)
#define MADV_WEAKORDER      (9)
#define MADV_STRONGORDER    (-9)

int madvise(void *__addr, size_t __len, int __advice);
int mlock(void const *__addr, size_t __len);
int munlock(void const *__addr, size_t __len);
int msync(void const *__addr, size_t __len, int __flags);

#define MS_ASYNC        0
#define MS_SYNC         1
#define MS_INVALIDATE   0

__END_DECLS
