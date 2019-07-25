#include "debug.h"
#include "export.h"

static write_debug_str_handler_t write_debug_str_vec;

void write_debug_str_set_handler(write_debug_str_handler_t handler)
{
    write_debug_str_vec = handler;
}

int write_debug_str(char const *str, intptr_t len)
{
    if (write_debug_str_vec)
        return write_debug_str_vec(str, len);
    return -1;
}

EXPORT void cpu_debug_break()
{
}
