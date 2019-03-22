#include "sys_module.h"
#include "errno.h"
#include "syscall/sys_module.h"
#include "elf64.h"
#include "mm.h"
#include "unique_ptr.h"

int sys_init_module(const char *module, size_t module_sz,
                    char const *module_name, const char *module_params)
{
    char kmname[256];

    if (unlikely(!mm_copy_user_str(kmname, module_name, sizeof(kmname))))
        return -int(errno_t::EFAULT);

    ext::unique_mmap<char> mem;

    if (!mem.mmap(nullptr, module_sz, PROT_READ | PROT_WRITE,
                   MAP_POPULATE, -1, 0))
        return -int(errno_t::ENOMEM);

    if (!mm_copy_user(mem, module, module_sz))
        return -int(errno_t::EFAULT);

    bool worked = modload_load_image(module, module_sz, kmname);

    return worked ? 0 : -int(errno_t::EINVAL);
}

int sys_delete_module(const char *name_user)
{
    return int(errno_t::ENOSYS);
}

int sys_query_module(const char *name_user, int which,
                     char *buf, size_t bufsize, size_t *ret)
{
    return int(errno_t::ENOSYS);
}

int sys_get_kernel_syms(kernel_sym *table)
{
    return int(errno_t::ENOSYS);
}
