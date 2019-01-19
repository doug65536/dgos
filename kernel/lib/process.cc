#include "process.h"
#include <inttypes.h>

#include "mm.h"
#include "fileio.h"
#include "elf64_decl.h"
#include "hash_table.h"
#include "stdlib.h"
#include "mutex.h"
#include "likely.h"
#include "fileio.h"
#include "errno.h"
#include "thread.h"
#include "vector.h"
#include "desc_alloc.h"
#include "cpu/control_regs.h"
#include "cpu/isr.h"
#include "contig_alloc.h"

union process_ptr_t {
    process_t *p;
    pid_t next;

    process_ptr_t()
        : p(nullptr)
    {
    }
};

static std::vector<process_ptr_t> processes;
static size_t process_count;
using processes_lock_type = std::mcslock;
using processes_scoped_lock = std::unique_lock<processes_lock_type>;
static processes_lock_type processes_lock;
static pid_t process_first_free;
static pid_t process_last_free;

process_t *process_t::add_locked(processes_scoped_lock const&)
{
    pid_t pid;
    size_t realloc_count = 0;

    process_t *process = new process_t;
    if (unlikely(!process))
        return nullptr;

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
            delete process;
            return nullptr;
        }

        if (unlikely(process_count == 2))
            processes[0].p = nullptr;
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
    processes_scoped_lock lock(processes_lock);
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
    process->argv = (char**)calloc(arg_count + 1, sizeof(*argv));
    process->env = (char**)calloc(env_count + 1, sizeof(*envp));

    for (size_t i = 0; i < arg_count; ++i)
        process->argv[i] = strdup(argv[i]);
    process->argv[arg_count] = nullptr;
    process->argc = arg_count;

    for (size_t i = 0; i < env_count; ++i)
        process->env[i] = strdup(envp[i]);
    process->env[env_count] = nullptr;
    process->envc = env_count;

    // Return the assigned PID
    *pid_result = process->pid;

    thread_t tid = thread_create(&process_t::start, process, 0, true);

    process->add_thread(tid);

    processes_scoped_lock lock(process->process_lock);
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
    // Attach this kernel thread to this process
    thread_set_process(-1, this);

    // Switch to user address space
    mmu_context = mm_new_process(this);

    // Simply load it for now
    Elf64_Ehdr hdr;

    file_t fd{file_open(path, O_RDONLY)};

    if (unlikely(!fd)) {
        printdbg("Failed to open executable %s\n", path);
        return -1;
    }

    ssize_t read_size;

    read_size = file_read(fd, &hdr, sizeof(hdr));
    if (unlikely(read_size != sizeof(hdr))) {
        printdbg("Failed to read ELF header\n");
        return -1;
    }

    // Allocate memory for program headers
    std::vector<Elf64_Phdr> program_hdrs;
    program_hdrs.resize(hdr.e_phnum);

    // Read program headers
    read_size = sizeof(Elf64_Phdr) * hdr.e_phnum;
    if (unlikely(read_size != file_pread(
                     fd,
                     program_hdrs.data(),
                     read_size,
                     hdr.e_phoff))) {
        printdbg("Failed to read program headers\n");
        return -1;
    }

    size_t last_region_st = ~size_t(0);
    size_t last_region_en = 0;

    // Map every section, just in case any pages overlap
    for (Elf64_Phdr& ph : program_hdrs) {
        // If it is not readable, writable or executable, ignore
        if ((ph.p_flags & (PF_R | PF_W | PF_X)) == 0)
            continue;

        // No memory size? Ignore
        if (ph.p_memsz == 0)
            continue;

        // See if it begins in reserved space
        if (intptr_t(ph.p_vaddr) < 0x400000) {
            printdbg("The virtual address is not in user address space\n");
            return -1;
        }

        // See if it overflows into kernel space
        if (intptr_t(ph.p_vaddr + ph.p_memsz) < 0) {
            printdbg("The section overflows into user space\n");
            return -1;
        }

        int page_prot = 0;

        if (ph.p_flags & PF_R)
            page_prot |= PROT_READ;

        // Unconditionally writable until loaded
        page_prot |= PROT_WRITE;
        if (ph.p_flags & PF_X)
            page_prot |= PROT_EXEC;

        // Skip pointless calls to mmap for little regions that overlap
        // previously reserved regions
        if (ph.p_vaddr >= last_region_st &&
                ph.p_vaddr + ph.p_memsz <= last_region_en)
            continue;

        // Update region reserved by last mapping
        last_region_st = ph.p_vaddr & -PAGESIZE;
        last_region_en = ((ph.p_vaddr + ph.p_memsz) + PAGESIZE - 1) & -PAGESIZE;

        if (unlikely(mmap((void*)last_region_st,
                          last_region_en - last_region_st, page_prot,
                          MAP_USER | MAP_NOCOMMIT, -1, 0) == MAP_FAILED)) {
            printdbg("Failed to reserve %#" PRIx64
                     " bytes of address space"
                     " with protection %d"
                     " at %#" PRIx64 "! \n",
                     ph.p_memsz, page_prot, ph.p_vaddr);
            return -1;
        }
    }

    // Read everything after mapping the memory
    for (Elf64_Phdr& ph : program_hdrs) {
        read_size = ph.p_filesz;
        if (ph.p_filesz > 0) {
            if (unlikely(read_size != file_pread(
                             fd, (void*)ph.p_vaddr,
                             read_size, ph.p_offset))) {
                printdbg("Failed to read program headers!\n");
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

        if (unlikely(mprotect((void*)ph.p_vaddr, ph.p_memsz, page_prot) < 0)) {
            printdbg("Failed to set page protection\n");
            return -1;
        }
    }

    // Initialize the stack

    size_t stack_size = 65536;
    void *stack = mmap(nullptr, stack_size, PROT_READ | PROT_WRITE,
                       MAP_STACK | MAP_USER, -1, 0);

    if (unlikely(stack == MAP_FAILED)) {
        printdbg("Failed to allocate user stack\n");
        return -1;
    }

    printdbg("process: allocated %zuKB stack at %#zx\n",
             stack_size >> 10, uintptr_t(stack));

    // Initialize the stack according to the ELF ABI
    //
    // lowest address
    // +---------------------------+--------------------------+
    // | argc                      | sizeof(uintptr_t)        |
    // | argv[]                    | sizeof(uintptr_t)*argc   |
    // | nullptr                   | sizeof(uintptr_t)        |
    // | envp[]                    | sizeof(uintptr_t)*envc   |
    // | nullptr                   | sizeof(uintptr_t)        |
    // | auxv[]                    | sizeof(auxv_t)*auxc      |
    // | zeroes                    | auxv_t filled with zeros |
    // | strings/structs for above | variable sized           |
    // +---------------------------+--------------------------+
    // highest address

    // Calculate size of variable sized area
    size_t info_sz;

    // Populate the stack
    void *stack_ptr = (char*)stack + stack_size;

    // Calculate the total size of environment string text
    info_sz = 0;
    for (size_t i = 0; i < envc; ++i) {
        size_t len = strlen(env[i]) + 1;
        info_sz += len;
    }

    // Calculate where the environment string text starts
    stack_ptr = (char*)stack_ptr - info_sz;
    char *env_ptr = (char*)stack_ptr;

    // Calculate the total size of argument string text
    info_sz = 0;
    for (size_t i = 0; i < argc; ++i) {
        size_t len = strlen(argv[i]) + 1;
        info_sz += len;
    }

    // Calculate where the argument string text starts
    stack_ptr = (char*)stack_ptr - info_sz;
    char *arg_ptr = (char*)stack_ptr;

    // Align the stack pointer
    stack_ptr = (void*)(uintptr_t(stack_ptr) & -sizeof(void*));

    // Calculate where the pointers to the environment strings start
    stack_ptr = (char**)env_ptr - (envc + 1);
    char **envp_ptr = (char**)stack_ptr;

    // Copy the environment strings and populate environment string pointers
    for (size_t i = 0; i < envc; ++i) {
        size_t len = strlen(env[i]) + 1;
        // Copy the string
        memcpy(env_ptr, env[i], len);
        // Write the pointer to the string
        envp_ptr[i] = env_ptr;
        // Advance string output pointer
        env_ptr += len;
    }

    // Calculate where the pointers to the argument strings start
    stack_ptr = envp_ptr - (argc + 1);
    char **argp_ptr = (char**)stack_ptr;

    // Copy the argument strings and populate argument string pointers
    for (size_t i = 0; i < envc; ++i) {
        size_t len = strlen(argv[i]) + 1;
        // Copy the string
        memcpy(arg_ptr, argv[i], len);
        // Write the pointer to the string
        argp_ptr[i] = arg_ptr;
        // Advance string output pointer
        arg_ptr += len;
    }

    // Push argv to the stack
    stack_ptr = (char**)stack_ptr - 1;
    mm_copy_user(stack_ptr, &argp_ptr, sizeof(argp_ptr));

    // Push argc to the stack
    stack_ptr = (uintptr_t*)stack_ptr - 1;
    mm_copy_user(stack_ptr, &argc, sizeof(argc));

    std::vector<auxv_t> auxent;

    if (!auxent.push_back({ auxv_t::AT_ENTRY, (void*)hdr.e_entry }))
        panic_oom();
    if (!auxent.push_back({ auxv_t::AT_PAGESZ, PAGESIZE }))
        panic_oom();
    if (!auxent.push_back({ auxv_t::AT_PHENT, hdr.e_phentsize }))
        panic_oom();
    if (!auxent.push_back({ auxv_t::AT_EXECFD, fd.release() }))
        panic_oom();

    // Open a stdin, stdout, and stderr
    file_open("/dev/conin", 0, 0);
    file_open("/dev/conout", 0, 0);
    file_open("/dev/conerr", 0, 0);

    processes_scoped_lock lock(processes_lock);
    state = state_t::running;
    lock.unlock();
    cond.notify_all();

    isr_sysret64(hdr.e_entry, uintptr_t(stack_ptr));

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

void process_t::destroy()
{
    free(path);
    path = nullptr;

    for (size_t i = 0; i < argc; ++i)
        free(argv[i]);
    free(argv);
    argv = nullptr;

    for (size_t i = 0; i < envc; ++i)
        free(env[i]);
    free(env);
    env = nullptr;

    delete (contiguous_allocator_t*)linear_allocator;
    linear_allocator = nullptr;
}

void process_t::exit(pid_t pid, int exitcode)
{
    // Kill all the threads...

    process_t *process_ptr = lookup(pid);

    process_ptr->exitcode = exitcode;
    thread_exit(exitcode);
}

bool process_t::add_thread(thread_t tid)
{
    return threads.push_back(tid);
}

// Returns true when the last thread exits
bool process_t::del_thread(thread_t tid)
{
    thread_list::iterator it = std::find(threads.begin(), threads.end(), tid);

    if (unlikely(it == threads.end()))
        return false;

    threads.erase(it);

    return threads.empty();
}

process_t *process_t::lookup(pid_t pid)
{
    if (unlikely(pid >= int(processes.size())))
        return nullptr;

    return (pid < 0)
            ? thread_current_process()
            : processes[pid].p;
}

process_t *process_t::init(uintptr_t mmu_context)
{
    process_t *process = process_t::add();
    process->mmu_context = mmu_context;
    return process;
}
