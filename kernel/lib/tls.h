#pragma once
#include "types.h"

typedef struct thread_env_t thread_env_t;
struct thread_env_t {
    // Compiler expects pointer thread environment to be
    // at fs:0, and expects it to be stored right after
    // the TLS.
    thread_env_t *self;
};

void tls_init(void);
size_t tls_size(void);
size_t tls_init_size(void);
void *tls_init_data(void);
