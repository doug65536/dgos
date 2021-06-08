#pragma once
#include "types.h"

__BEGIN_DECLS

KERNEL_API intptr_t binary_search(
        void *va, size_t count, size_t item_size,
        void const *k,
        int (*cmp)(void const *, void const *, void *),
        void *c, int unique);

__END_DECLS
