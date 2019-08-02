#pragma once
#include "types.h"
#include "vector.h"
#include "cxxstring.h"

__BEGIN_DECLS

struct plt_stub_data_t;

extern void __module_dynlink_plt_thunk();

void __module_dynamic_linker(plt_stub_data_t *data);

void modload_init(void);

class module_t;

module_t *modload_load(char const *path, bool run = true);
module_t *modload_load_image(void const *image, size_t image_sz,
                             char const *module_name,
                             std::vector<std::string> parameters,
                             errno_t *ret_errno = nullptr);

int modload_run(module_t *module);

int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle);

__END_DECLS
