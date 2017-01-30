#include "tls.h"
#include "printk.h"
#include "string.h"
#include "cpu/control_regs.h"

extern char ___main_tls_bottom[];
extern thread_env_t *___main_teb_ptr;

extern uintptr_t ___tls_size;
extern void *___tls_init_data_ptr;
extern void *___tls_main_tls_bottom_ptr;

size_t tls_size(void)
{
    return ___tls_size;
}

void *tls_init_data(void)
{
    return ___tls_init_data_ptr;
}

void tls_init(void)
{
    // Initialize the statically allocated main thread's TLS
    memcpy(___tls_main_tls_bottom_ptr, ___tls_init_data_ptr,
           ___tls_size);

    ___main_teb_ptr->self = ___main_teb_ptr;

    cpu_set_fsbase(___main_teb_ptr);
}
