#pragma once
#include "types.h"

__BEGIN_DECLS

typedef int (*module_entry_fn_t)();

struct plt_stub_data_t;

extern void __module_dynlink_plt_thunk();

void __module_dynamic_linker(plt_stub_data_t *data);

void modload_init(void);

class module_t;

module_t *modload_load(char const *path, bool run = true);
module_t *modload_load_image(void const *image, size_t image_sz, const char *module_name,
                             bool run = true);

int modload_run(module_t *module);

__END_DECLS
