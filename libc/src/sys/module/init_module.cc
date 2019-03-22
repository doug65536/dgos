#include <stdlib.h>
#include <sys/module.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <string.h>

int init_module(void const *module, size_t module_sz, char const *module_name,
                struct module *mod_user)
{
    long err = syscall4(long(module), long(module_sz),
                        long(module_name), long(mod_user),
                        SYS_init_module);

    if (err >= 0)
        return int(err);

    errno = int(-err);
    return -1;
}
