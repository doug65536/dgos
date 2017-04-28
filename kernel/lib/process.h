#pragma once

typedef int pid_t;
struct process_t;

#include "thread.h"

process_t *process_init(uintptr_t mmu_context);

// Load and execute the specified program
int process_spawn(pid_t * pid,
                  char const * path,
                  char const * const * argv,
                  char const * const * envp);

void process_remove(process_t *process);

void *process_get_allocator();
void process_set_allocator(void *allocator);
