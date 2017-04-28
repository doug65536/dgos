#include "process.h"

#include "fileio.h"
#include "mm.h"
#include "elf64_decl.h"
#include "hash_table.h"
#include "stdlib.h"
#include "cpu/spinlock.h"
#include "likely.h"
#include "fileio.h"
#include "errno.h"
#include "thread.h"

union process_ptr_t {
    process_t *p;
    pid_t next;
};

struct process_t {
    pid_t pid;
    char *path;
    char **args;
    char **env;
    uintptr_t mmu_context;
    void *linear_allocator;
};

static process_ptr_t *processes;
static size_t process_count;
static spinlock_t processes_lock;
static pid_t process_first_free;
static pid_t process_last_free;

static process_t *process_add_locked(void)
{
    pid_t pid;
    size_t realloc_count = 0;

    process_t *process = (process_t*)calloc(1, sizeof(process_t));
    if (unlikely(!process))
        return 0;

    if (process_first_free != 0) {
        // Reuse oldest pid
        pid = process_first_free;
        if (process_first_free == process_last_free)
            process_last_free = 0;
        process_first_free = processes[process_first_free].next;
    } else if (process_count) {
        // New pid
        pid = process_count;
        realloc_count = process_count + 1;
    } else {
        //
        pid = 1;
        realloc_count = 2;
    }

    if (realloc_count) {
        // Expand process list
        process_ptr_t *new_processes = (process_ptr_t*)realloc(
                    processes, sizeof(*processes) *
                    (process_count + 1));
        if (!new_processes) {
            free(process);
            return 0;
        }

        processes = new_processes;

        if (unlikely(process_count == 2))
            processes[0].p = 0;
    }
    processes[pid].p = process;
    process->pid = pid;

    return process;
}

void process_remove(process_t *process)
{
    if (process_last_free) {
        processes[process_last_free].next = process->pid;
        process_last_free = process->pid;
    }
}

static process_t *process_add(void)
{
    spinlock_lock(&processes_lock);
    process_t *result = process_add_locked();
    spinlock_unlock(&processes_lock);
    return result;
}

int process_spawn(pid_t * pid_result,
				  char const * path,
				  char const * const * argv,
				  char const * const * envp)
{
    process_t *process = process_add();

    process->path = strdup(path);

    // Count the arguments
    size_t arg_count = 0;
    while (argv && argv[arg_count++]);

    // Count the environment variables
    size_t env_count = 0;
    while (envp && envp[env_count++]);

    // Make a copy of the arguments and environment variables
    process->args = (char**)calloc(arg_count + 1, sizeof(*argv));
    process->env = (char**)calloc(env_count + 1, sizeof(*envp));

    for (size_t i = 0; i < arg_count; ++i)
        process->args[i] = strdup(argv[i]);
    process->args[arg_count] = 0;

    for (size_t i = 0; i < env_count; ++i)
        process->env[i] = strdup(envp[i]);
    process->env[env_count] = 0;

    // Return the assigned PID
    *pid_result = process->pid;

    process->mmu_context = mm_new_process();

    return 0;
}

void *process_get_allocator()
{
    process_t *process = thread_current_process();
    return process->linear_allocator;
}

void process_set_allocator(void *allocator)
{
    process_t *process = thread_current_process();
    assert(process->linear_allocator == nullptr);
    process->linear_allocator = allocator;
}

process_t *process_init(uintptr_t mmu_context)
{
    process_t *process = process_add();
    process->mmu_context = mmu_context;
    return process;
}
