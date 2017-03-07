#include "process.h"

#include "fileio.h"
#include "mm.h"
#include "elf64_decl.h"
#include "hash_table.h"
#include "stdlib.h"
#include "cpu/spinlock.h"
#include "likely.h"
#include "fileio.h"

typedef union process_ptr_t {
    process_t *p;
    pid_t next;
} process_ptr_t;

struct process_t {
    pid_t pid;
    char *path;
    char **args;
    char **env;
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

    process_t *process = calloc(1, sizeof(process_t));
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
        process_ptr_t *new_processes = realloc(
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

int process_spawn(pid_t * restrict pid_result,
                  char const * restrict path,
                  char const * const * restrict argv,
                  char const * const * restrict envp)
{
    process_t *process = process_add();

    process->path = strdup(path);

    size_t arg_count = 0;
    while (argv && argv[arg_count++]);

    size_t env_count = 0;
    while (envp && envp[env_count++]);

    process->args = calloc(arg_count + 1, sizeof(*argv));
    process->env = calloc(env_count + 1, sizeof(*envp));

    for (size_t i = 0; i < arg_count; ++i)
        process->args[i] = strdup(argv[i]);
    process->args[arg_count] = 0;

    for (size_t i = 0; i < env_count; ++i)
        process->env[i] = strdup(argv[i]);
    process->env[env_count] = 0;

    *pid_result = process->pid;

    return 0;
}
