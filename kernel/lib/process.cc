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
#include "vector.h"
#include "desc_alloc.h"

union process_ptr_t {
    process_t *p;
    pid_t next;
};

static process_ptr_t *processes;
static size_t process_count;
static spinlock processes_lock;
static pid_t process_first_free;
static pid_t process_last_free;

static process_t *process_add_locked(unique_lock<spinlock> const&)
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
    unique_lock<spinlock> lock(processes_lock);
    process_t *result = process_add_locked(lock);
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

    // Simply load it for now
    Elf64_Ehdr hdr;

    file_t fd = file_open(path, O_RDONLY);

    ssize_t read_size;

    read_size = file_read(fd, &hdr, sizeof(hdr));
    if (read_size != sizeof(hdr))
        return -1;

    // Allocate memory for program headers
    vector<Elf64_Phdr> program_hdrs;
    program_hdrs.resize(hdr.e_phnum);

    // Read program headers
    read_size = sizeof(Elf64_Phdr) * hdr.e_phnum;
    if (read_size != file_pread(
                fd,
                program_hdrs.data(),
                read_size,
                hdr.e_phoff))
        return -1;

    uintptr_t top_addr;

    for (Elf64_Phdr& ph : program_hdrs) {
        // If it is not readable, writable or executable, ignore
        if ((ph.p_flags & (PF_R | PF_W | PF_X)) == 0)
            continue;

        if (ph.p_memsz == 0)
            continue;

        // See if it grossly overflows into kernel space
        if (intptr_t(ph.p_vaddr + ph.p_memsz) < 0)
            return -1;

        int page_prot = 0;

        if (ph.p_flags & PF_R)
            page_prot |= PROT_READ;
        if (ph.p_flags & PF_W)
            page_prot |= PROT_WRITE;
        if (ph.p_flags & PF_X)
            page_prot |= PROT_EXEC;

        void *mem = mmap((void*)ph.p_vaddr,
                         ph.p_memsz, page_prot,
                         MAP_USER | MAP_POPULATE, -1, 0);

        read_size = ph.p_filesz;
        if (ph.p_filesz > 0) {
            if (read_size != file_pread(
                        fd,
                        mem,
                        read_size,
                        ph.p_offset)) {
                return -1;
            }
        }
    }

    // Initialize the stack

    thread_create(thread_fn_t(uintptr_t(hdr.e_entry)),
                  nullptr, nullptr, 0);

    return process->pid;
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
