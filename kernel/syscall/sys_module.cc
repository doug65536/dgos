#include "sys_module.h"
#include "errno.h"
#include "syscall/sys_module.h"
#include "elf64.h"
#include "mm.h"
#include "unique_ptr.h"
#include "device/pci.h"
#include "thread.h"
#include "process.h"

int sys_init_module(char const *module, size_t module_sz,
                    char const *module_name, char const *module_params)
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

int sys_delete_module(char const *name_user)
{
    return int(errno_t::ENOSYS);
}

int sys_query_module(char const *name_user, int which,
                     char *buf, size_t bufsize, size_t *ret)
{
    return int(errno_t::ENOSYS);
}

int sys_get_kernel_syms(kernel_sym *table)
{
    return int(errno_t::ENOSYS);
}

int sys_probe_pci_for(int32_t vendor, int32_t device, int32_t devclass, int32_t subclass, int32_t prog_if)
{
    process_t *proc = thread_current_process();

    if (unlikely(proc->uid != 0))
        return -int(errno_t::EACCES);

    pci_dev_iterator_t iter;
    if (pci_enumerate_begin(&iter, devclass, subclass, vendor, devclass)) {
        do {
            if (prog_if < 0 || iter.config.prog_if == prog_if)
                return 1;
        } while(pci_enumerate_next(&iter));
    }
    return 0;
}
