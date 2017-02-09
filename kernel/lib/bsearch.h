#pragma once
#include "types.h"

intptr_t binary_search(void *va, size_t count, size_t item_size,
                       const void *k,
                       int (*cmp)(const void *,
                                  const void *,
                                  void *),
                       void *c, int unique);
