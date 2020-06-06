#include <stdlib.h>
#include <sys/module.h>
#include <errno.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <string.h>

int init_module(void const *module, ptrdiff_t module_sz,
                char const *module_name, struct module *mod_user,
                char const *parameters, char *ret_needed)
{
    long status = syscall6(uintptr_t(module), module_sz,
                           uintptr_t(module_name),
                           uintptr_t(mod_user),
                           uintptr_t(parameters),
                           uintptr_t(ret_needed),
                           SYS_init_module);

    if (likely(status >= 0))
        return int(status);

    errno = int(-status);
    return -1;
}
