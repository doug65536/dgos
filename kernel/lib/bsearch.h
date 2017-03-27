#pragma once
#include "types.h"

intptr_t binary_search(void *va, size_t count, size_t item_size,
                       void const *k,
                       int (*cmp)(void const *,
                                  void const *,
                                  void *),
                       void *c, int unique);
