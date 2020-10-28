#pragma once
#include "types.h"
#include "vector.h"
#include "cxxstring.h"

__BEGIN_DECLS

struct plt_stub_data_t;

extern void __module_dynlink_plt_thunk();

void __module_dynamic_linker(plt_stub_data_t *data);

void modload_init(void);

class KERNEL_API module_t;

module_t *modload_load(char const *path, bool run = true);
module_t *modload_load_image(void const *image, size_t image_sz,
                             char const *module_name,
                             ext::vector<ext::string> parameters,
                             char *ret_needed,
                             errno_t *ret_errno = nullptr);

int modload_run(module_t *module);

KERNEL_API module_t *modload_closest(ptrdiff_t address);
KERNEL_API ext::string const& modload_get_name(module_t *module);
KERNEL_API uintptr_t modload_get_base_adj(module_t *module);
KERNEL_API uintptr_t modload_get_vaddr_min(module_t *module);
KERNEL_API size_t modload_get_size(module_t *module);
KERNEL_API size_t modload_get_count();
KERNEL_API module_t *modload_get_index(size_t i);

KERNEL_API int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle);

void *__tls_get_addr(void *a, void *b);

__END_DECLS
