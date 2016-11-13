#include "time.h"

static uint64_t time_ms_dummy(void);

uint64_t (*time_ms)(void) = time_ms_dummy;

// Provides a handler for time_ms until the timer is initialized
static uint64_t time_ms_dummy(void)
{
    return 0;
}
