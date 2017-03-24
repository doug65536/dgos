#pragma once
#include "types.h"

// Callback function pointer
typedef void (*cpu_broadcast_handler_t)(void *);

size_t cpu_broadcast_create(void);
void cpu_broadcast_service(int intr, size_t slot);
void cpu_broadcast_message(int intr, size_t slot, int other_only,
                           cpu_broadcast_handler_t handler,
                           void *data, size_t size, int unique);
