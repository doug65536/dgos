#include "tls.h"
#include "printk.h"
#include "string.h"
#include "control_regs.h"

typedef struct thread_env_t thread_env_t;
struct thread_env_t {
    // Compiler expects pointer thread environment to be
    // at fs:0, and expects it to be stored right after
    // the TLS.
    thread_env_t *self;
};

extern char ___main_tls_bottom[];
extern thread_env_t ___main_teb;
extern thread_env_t ___main_teb_end;

extern char ___tdata_st[];
extern char ___tbss_en[];

void tls_init(void)
{
    // ??? Why doesn't this work
    if (&___main_teb_end != &___main_teb + 1) {
        panic("Linker script wrong, &__main_teb_end != &__main_teb + 1");
    }

    if (((char*)&___main_teb - ___main_tls_bottom) !=
            ___tbss_en - ___tdata_st) {
        panic("Linker script wrong, wrong size for statically allocated TLS");
    }

    // Initialize the statically allocated main thread's TLS
    memcpy(___main_tls_bottom, ___tdata_st,
           ___tbss_en - ___tdata_st);

    ___main_teb.self = &___main_teb;

    cpu_set_fsgsbase(&___main_teb, 0);
}
