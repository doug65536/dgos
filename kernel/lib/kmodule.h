#pragma once
#include "types.h"

__BEGIN_DECLS

using module_entry_fn_t = int(*)(int argc, char const **argv);

int module_main(int argc, char const * const * argv);

__END_DECLS
