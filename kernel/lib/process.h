#pragma once

#include "thread.h"

typedef int pid_t;

typedef struct process_t process_t;

// Load and execute the specified program
int process_spawn(pid_t * restrict pid,
                  char const * restrict path,
                  char const * const * restrict argv,
                  char const * const * restrict envp);

void process_remove(process_t *process);
