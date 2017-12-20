#pragma once
#include "types.h"

extern "C" {

void *sys_mmap(void *addr, size_t len, int prot, 
               int flags, int fd, off_t offset);

int sys_mprotect(void *addr, size_t len, int prot);

int sys_munmap(void *addr, size_t size);

void *sys_mremap(
        void *old_address,
        size_t old_size,
        size_t new_size,
        int flags,
        void *new_address);

int sys_madvise(void *addr, size_t len, int advice);

int sys_msync(void const *addr, size_t len, int flags);

int sys_mlock(void const *__addr, size_t __len);

int sys_munlock(void const *__addr, size_t __len);

int sys_mlockall(int __flags);

int sys_munlockall(void);

}
