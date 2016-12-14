#pragma once
#include "types.h"

intptr_t binary_search(void *va, size_t count, size_t item_size,
                     void *k, int (*cmp)(void *v, void *k, void *c),
                     void *c);
