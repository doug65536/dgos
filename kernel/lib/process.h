#pragma once

#include "thread.h"

typedef int pid_t;

struct process_t;

// Load and execute the specified program
int process_spawn(pid_t * pid,
                  char const * path,
                  char const * const * argv,
                  char const * const * envp);

void process_remove(process_t *process);
