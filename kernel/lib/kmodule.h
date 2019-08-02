#pragma once
#include "types.h"

__BEGIN_DECLS

using module_entry_fn_t = int(*)(int __argc, char const **__argv);

int module_main(int __argc, char const * const * __argv);

void __module_register_frame(void const * const *__module_dso_handle,
                             void *__frame);

__END_DECLS
