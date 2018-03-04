#include "hash_table.h"
#include "stdlib.h"
#include "hash.h"
#include "string.h"
#include "utility.h"
#include "refcount.h"

// Open addressing hash table
// When deleting an item with a non-null next slot,
// we "punch a hole" by setting the item pointer to
// (void*)1. We don't decrease the count when punching
// a hole. We continue probe when encountering a hole
// we really delete items if the next slot is null.
// We don't increase count when inserting at a hole.
// If holes accumulate, this causes a rehash to occur
// appropriately.












