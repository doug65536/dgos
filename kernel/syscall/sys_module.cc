#include "sys_module.h"
#include "errno.h"
#include "syscall/sys_module.h"
#include "elf64.h"
#include "mm.h"
#include "unique_ptr.h"
#include "device/pci.h"
#include "thread.h"
#include "process.h"
#include "user_mem.h"

int sys_init_module(char const *module, ptrdiff_t module_sz,
                    char const *module_name, struct module *mod_user,
                    char const *module_params,
                    char *ret_needed)
{
    char kmname[256];

    if (unlikely(mm_copy_user_str(kmname, module_name, sizeof(kmname)) < 0))
        return -int(errno_t::EFAULT);

    ext::unique_mmap<char> mem;

    if (unlikely(!mem.mmap(nullptr, module_sz,
                           PROT_READ | PROT_WRITE,
                           MAP_POPULATE, -1, 0)))
        return -int(errno_t::ENOMEM);

    if (unlikely(!mm_copy_user(mem, module, module_sz)))
        return -int(errno_t::EFAULT);

    // Maximum of 64KB of parameters
    mm_copy_string_result_t parameter_buffer;

    if (module_params)
        parameter_buffer = mm_copy_user_string(module_params, 64 << 10);

    if (unlikely(module_params && !parameter_buffer.second))
        return -int(errno_t::EFAULT);

    std::vector<std::string> parameters;

    bool in_squote = false;
    bool in_dquote = false;
    bool in_escape = false;
    for (char const *it = parameter_buffer.first.data(); it && *it; ++it) {
        int ch = -1;
        if (!in_escape) {
            switch (*it) {
            case '\\':
                in_escape = true;
                continue;
            case '"':
                if (!in_squote) {
                    in_dquote = !in_dquote;
                    continue;
                }
                break;
            case '\'':
                if (!in_dquote) {
                    in_squote = !in_squote;
                    continue;
                }
                break;
            case ' ':
                // Space has no special meaning inside a single or double quote
                if (in_squote || in_dquote)
                    break;

                // Ignore whitespace before first parameter
                if (parameters.empty())
                    continue;

                // If there is a parameter there, and it is not empty, then
                // the space indicates we are done collecting that one, start
                // a new one
                if (!parameters.empty() && !parameters.back().empty())
                    parameters.emplace_back();
            }

            ch = *it;
        } else {
            // In escape
            switch (*it) {
            case 'e': ch = '\x1b'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'b': ch = '\b'; break;
            case '\\': ch = '\\'; break;
            default: ch = *it;
            }
        }

        // If there is no parameter yet, add an empty one
        if (parameters.empty())
            parameters.emplace_back();

        // Append character to last parameter
        parameters.back().push_back(ch);
    }

    errno_t err = errno_t::OK;

    bool worked = modload_load_image(module, module_sz, kmname,
                                     std::move(parameters),
                                     ret_needed, &err);

    return worked ? 0 : -int(err);
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

int sys_probe_pci_for(int32_t vendor, int32_t device,
                      int32_t devclass, int32_t subclass,
                      int32_t prog_if)
{
    process_t *proc = thread_current_process();

    if (unlikely(proc->uid != 0))
        return -int(errno_t::EACCES);

    pci_dev_iterator_t iter;
    if (pci_enumerate_begin(&iter, devclass, subclass, vendor, device)) {
        do {
            // Attempt to reject by prof_if mismatch
            if (prog_if >= 0 && iter.config.prog_if != prog_if)
                continue;

            // Made it this far? It is a match!
            return 1;
        } while(pci_enumerate_next(&iter));
    }
    return 0;
}
