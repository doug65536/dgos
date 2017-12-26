#include "process.h"

#include "mm.h"
#include "fileio.h"
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
#include "cpu/control_regs.h"
#include "cpu/isr.h"

union process_ptr_t {
    process_t *p;
    pid_t next;

    process_ptr_t()
        : p(nullptr)
    {
    }
};

static vector<process_ptr_t> processes;
static size_t process_count;
static spinlock processes_lock;
static pid_t process_first_free;
static pid_t process_last_free;

process_t *process_t::add_locked(unique_lock<spinlock> const&)
{
    pid_t pid;
    size_t realloc_count = 0;

    process_t *process = new process_t;
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
        pid = process_count++;
        realloc_count = process_count;
    } else {
        pid = 1;
        realloc_count = 2;
        process_count = 2;
    }

    if (realloc_count) {
        // Expand process list
        if (!processes.resize(realloc_count)) {
            free(process);
            return nullptr;
        }

        if (unlikely(process_count == 2))
            processes[0].p = 0;
    }
    processes[pid].p = process;
    process->pid = pid;

    process->state = state_t::starting;

    return process;
}

void process_t::remove()
{
    if (process_last_free) {
        processes[process_last_free].next = pid;
        process_last_free = pid;
    }
}

process_t *process_t::add()
{
    unique_lock<spinlock> lock(processes_lock);
    process_t *result = process_t::add_locked(lock);
    return result;
}

int process_t::spawn(pid_t * pid_result,
                  char const * path,
                  char const * const * argv,
                  char const * const * envp)
{
    *pid_result = -1;

    process_t *process = process_t::add();

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

    thread_create(&process_t::start, process, nullptr, 0, false);

    unique_lock<spinlock> lock(process->process_lock);
    while (process->state == process_t::state_t::starting)
        process->cond.wait(lock);

    return process->state == process_t::state_t::running
            ? 0
            : int(errno_t::EFAULT);
}

int process_t::start(void *process_arg)
{
    return ((process_t*)process_arg)->start();
}

int process_t::start()
{
    thread_set_process(-1, this);

    mmu_context = mm_new_process(this);

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
        // Unconditionally writable until loaded
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

    // Make read only pages read only
    for (Elf64_Phdr& ph : program_hdrs) {
        int page_prot = 0;

        // Ignore the region if it should be writable
        if (ph.p_flags & PF_W)
            continue;

        if (ph.p_flags & PF_R)
            page_prot |= PROT_READ;
        if (ph.p_flags & PF_X)
            page_prot |= PROT_EXEC;

        mprotect((void*)ph.p_vaddr, ph.p_memsz, page_prot);
    }

    // Initialize the stack

    size_t stack_size = 65536;
    void *stack = mmap(0, stack_size, PROT_READ | PROT_WRITE,
                       MAP_STACK | MAP_USER, -1, 0);

    unique_lock<spinlock> lock(processes_lock);
    state = state_t::running;
    lock.unlock();
    cond.notify_all();

    isr_sysret64(hdr.e_entry, (uintptr_t)stack + stack_size);

    return pid;
}

void *process_t::get_allocator()
{
    process_t *process = thread_current_process();
    return process->linear_allocator;
}

void process_t::set_allocator(void *allocator)
{
    assert(linear_allocator == nullptr);
    linear_allocator = allocator;
}

process_t *process_t::init(uintptr_t mmu_context)
{
    process_t *process = process_t::add();
    process->mmu_context = mmu_context;
    return process;
}
