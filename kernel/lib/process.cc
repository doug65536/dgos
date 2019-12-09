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
#include "cpu/isr.h"
#include "contig_alloc.h"
#include "user_mem.h"
#include "cpu/except_asm.h"
#include "utility.h"

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
using processes_lock_type = ext::mcslock;
using processes_scoped_lock = std::unique_lock<processes_lock_type>;
static processes_lock_type processes_lock;
static pid_t process_first_free;
static pid_t process_last_free;

process_t *process_t::add_locked(processes_scoped_lock const&)
{
    pid_t pid;
    size_t realloc_count = 0;

    process_t *process = new (std::nothrow) process_t();
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
                  std::string path,
                  std::vector<std::string> argv,
                  std::vector<std::string> env)
{
    *pid_result = -1;

    process_t *process = process_t::add();

    process->path = std::move(path);
    process->argv = std::move(argv);
    process->env = std::move(env);

    // Return the assigned PID
    *pid_result = process->pid;


    thread_t tid = thread_create(&process_t::run, process, 0,
                                 true, true);

    if (unlikely(tid < 0))
        return int(errno_t::ENOMEM);

    if (unlikely(!process->add_thread(tid)))
        return int(errno_t::ENOMEM);

    // Wait for it to finish starting
    processes_scoped_lock lock(process->process_lock);
    while (process->state == process_t::state_t::starting)
        process->cond.wait(lock);

    return (process->state == process_t::state_t::running ||
            process->state == process_t::state_t::exited)
            ? 0
            : int(errno_t::EFAULT);
}

int process_t::run(void *process_arg)
{
    return ((process_t*)process_arg)->run();
}

// Hack to reuse module loader symbol auto-load hook for processes too
extern void modload_load_symbols(char const *path, uintptr_t text_addr,
                                 uintptr_t base_addr);

// Returns when program exits
int process_t::run()
{
    // Attach this kernel thread to this process
    thread_set_process(-1, this);

    // Switch to user address space
    mmu_context = mm_new_process(this);

    // Simply load it for now
    Elf64_Ehdr hdr;

    // Open a stdin, stdout, and stderr
    int fd_i = file_open("/dev/conin", 0, 0);
    //int fd_o = file_open("/dev/conout", 0, 0);
    //int fd_e = file_open("/dev/conerr", 0, 0);

    assert(fd_i == 0);
    //assert(fd_o == 1);
    //assert(fd_e == 2);

    file_t fd{file_open(path.c_str(), O_RDONLY)};

    if (unlikely(!fd)) {
        printdbg("Failed to open executable %s\n", path.c_str());
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
    if (unlikely(!program_hdrs.resize(hdr.e_phnum))) {
        printdbg("Failed to allocate memory for program headers\n");
        return -1;
    }

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
    size_t last_region_sz;

    uintptr_t first_exec = UINTPTR_MAX;

    // Map every section, just in case any pages overlap
    for (Elf64_Phdr& ph : program_hdrs) {
        // If it is not loaded, ignore
        if (ph.p_type != PT_LOAD)
            continue;

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
        if (ph.p_flags & PF_X) {
            if (first_exec == UINTPTR_MAX)
                first_exec = ph.p_vaddr;
            page_prot |= PROT_EXEC;
        }

        // Skip pointless calls to mmap for little regions that overlap
        // previously reserved regions
        if (ph.p_vaddr >= last_region_st &&
                ph.p_vaddr + ph.p_memsz <= last_region_en)
            continue;

        // Update region reserved by last mapping
        last_region_st = ph.p_vaddr & -PAGESIZE;
        last_region_en = ((ph.p_vaddr + ph.p_memsz) + PAGESIZE - 1) & -PAGESIZE;
        last_region_sz = last_region_en - last_region_st;

        if (unlikely(last_region_sz &&
                     mmap((void*)last_region_st, last_region_sz, page_prot,
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
        // If it is not loaded, ignore
        if (ph.p_type != PT_LOAD)
            continue;

        read_size = ph.p_filesz;
        if (likely(ph.p_filesz > 0)) {
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
        // If it is not loaded, ignore
        if (ph.p_type != PT_LOAD)
            continue;

        // Can't do anything if the size is zero
        if (unlikely(ph.p_memsz == 0))
            continue;

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

    // Find TLS
    for (Elf64_Phdr& ph : program_hdrs) {
        // If it is a TLS header, remember the range
        if (unlikely(ph.p_type == PT_TLS)) {
            tls_addr = ph.p_vaddr;
            tls_msize = ph.p_memsz;
            tls_fsize = ph.p_filesz;
        }
    }

    modload_load_symbols(path.c_str(), first_exec, 0);

    // Initialize the stack

    size_t stack_size = 65536;
    char *stack_memory = (char*)mmap(
                nullptr, stack_size, PROT_NONE,
                MAP_STACK | MAP_NOCOMMIT | MAP_USER, -1, 0);

    if (unlikely(stack_memory == MAP_FAILED)) {
        printdbg("Failed to allocate user stack\n");
        return -1;
    }

    // Guard page
    char *stack = stack_memory + PAGE_SIZE;
    stack_size -= PAGE_SIZE;

    // Permit access to area excluding guard area
    if (unlikely(mprotect(stack, stack_size, PROT_READ | PROT_WRITE) < 0)) {
        printdbg("Failed set page protetions on user stack\n");
        return -1;
    }

    // Commit first stack page
    if (unlikely(madvise(stack + stack_size - PAGE_SIZE,
                         PAGE_SIZE, MADV_WILLNEED) < 0)) {
        printdbg("Failed to commit initial page in user stack\n");
        return -1;
    }

    printdbg("process: allocated %zuKB stack at %#zx\n",
             stack_size >> 10, uintptr_t(stack));

    // Initialize main thread TLS
    static_assert(sizeof(uintptr_t) == sizeof(void*), "Unexpected size");
    // The space for the uintptr_t is for the pointer to itself at TLS offset 0
    size_t tls_vsize = PAGE_SIZE + sizeof(uintptr_t) + tls_msize + PAGE_SIZE;
    void *tls = mmap(nullptr, tls_vsize, PROT_NONE, MAP_USER, -1, 0);
    if (tls == MAP_FAILED)
        return -1;

    // 4KB guard region around TLS
    uintptr_t tls_area = uintptr_t(tls) + PAGESIZE;

    if (unlikely(!mm_is_user_range((void*)tls_area,
                                   sizeof(uintptr_t) + tls_msize)))
        return -1;

    if (unlikely(mprotect((void*)tls_area, tls_msize + sizeof(uintptr_t),
                          PROT_READ | PROT_WRITE) < 0))
        return -1;

    // Explicitly commit faster than taking demand faults
    if (unlikely(madvise((void*)tls_area, tls_msize + sizeof(uintptr_t),
                         MADV_WILLNEED) < 0))
        return -1;

    // Copy template into TLS area
    if (unlikely(!mm_copy_user((char*)tls_area, (void*)tls_addr, tls_fsize)))
        return -1;

    // Zero fill the region not specified in the file
    if (unlikely(!mm_copy_user((char*)tls_area + tls_fsize, nullptr,
                               tls_msize - tls_fsize)))
        return -1;

    /// TLS uses negative offsets from the end of the TLS data, and a
    /// pointer to the TLS is located at the TLS base address.
    ///
    ///    /\       +-----------------+
    ///    |  +-----| pointer to self |
    ///    |  +---->+-----------------+ <--- TLS base address (FSBASE)
    ///    +        |                 |
    ///    +        |    TLS  data    | <--- copied from TLS template
    ///    +        |                 |
    ///  addr       +-----------------+ <--- mmap allocation for TLS
    ///

    // The TLS pointer here points to the end of the TLS, which is where
    // the pointer to itself is located
    uintptr_t tls_ptr = tls_area + tls_msize;
    // Patch the pointer to itself into the TLS area
    if (unlikely(!mm_copy_user((void*)tls_ptr, &tls_ptr, sizeof(tls_ptr))))
        return -1;

    // Point fs at it
    thread_set_fsbase(thread_get_id(), tls_ptr);

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
    void *stack_ptr = stack + stack_size;

    // Calculate the total size of environment string text
    info_sz = 0;
    for (size_t i = 0; i < env.size(); ++i) {
        size_t len = env[i].length() + 1;
        info_sz += len;
    }

    // Calculate where the environment string text starts
    stack_ptr = (char*)stack_ptr - info_sz;
    char *env_ptr = (char*)stack_ptr;

    // Calculate the total size of argument string text
    info_sz = 0;
    for (size_t i = 0; i < argv.size(); ++i) {
        size_t len = argv[i].length() + 1;
        info_sz += len;
    }

    // Calculate where the argument string text starts
    stack_ptr = (char*)stack_ptr - info_sz;
    char *arg_ptr = (char*)stack_ptr;

    // Align the stack pointer
    stack_ptr = (void*)(uintptr_t(stack_ptr) & -sizeof(void*));

    // Calculate where the pointers to the environment strings start
    stack_ptr = (char**)env_ptr - (env.size() + 1);
    char **envp_ptr = (char**)stack_ptr;

    // Copy the environment strings and populate environment string pointers
    for (size_t i = 0; i < env.size(); ++i) {
        size_t len = env[i].length() + 1;
        // Copy the string
        memcpy(env_ptr, env[i].c_str(), len);
        // Write the pointer to the string
        envp_ptr[i] = env_ptr;
        // Advance string output pointer
        env_ptr += len;
    }

    // Calculate where the pointers to the argument strings start
    stack_ptr = envp_ptr - (argv.size() + 1);
    char **argp_ptr = (char**)stack_ptr;

    // Copy the argument strings and populate argument string pointers
    for (size_t i = 0; i < env.size(); ++i) {
        size_t len = argv[i].length() + 1;
        // Copy the string
        memcpy(arg_ptr, argv[i].c_str(), len);
        // Write the pointer to the string
        argp_ptr[i] = arg_ptr;
        // Advance string output pointer
        arg_ptr += len;
    }

    // Push argv to the stack
    stack_ptr = (char**)stack_ptr - 1;
    if (unlikely(!mm_copy_user(stack_ptr, &argp_ptr, sizeof(argp_ptr))))
        return -1;

    // Push argc to the stack
    stack_ptr = (uintptr_t*)stack_ptr - 1;
    int argc = argv.size();
    if (!mm_copy_user(stack_ptr, &argc, sizeof(argc)))
        return -1;

    std::vector<auxv_t> auxent;

    if (unlikely(!auxent.push_back({ auxv_t::AT_ENTRY, (void*)hdr.e_entry })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_PAGESZ, PAGESIZE })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_PHENT, hdr.e_phentsize })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_EXECFD, fd.release() })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_UID, 0L })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_EUID, 0L })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_GID, 0L })))
        panic_oom();
    if (unlikely(!auxent.push_back({ auxv_t::AT_EGID, 0L })))
        panic_oom();

    processes_scoped_lock lock(processes_lock);
    state = state_t::running;
    lock.unlock();
    cond.notify_all();

    return enter_user(hdr.e_entry, uintptr_t(stack_ptr));
}

int process_t::enter_user(uintptr_t ip, uintptr_t sp)
{
    //
    if (!__setjmp(&exit_jmpbuf)) {
        // First time
        isr_sysret64(ip, sp);
        __builtin_trap();
    }

    // exiting program continues here
    return exitcode;
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
    std::string empty_path;
    empty_path.swap(path);

    std::vector<std::string> empty_argv;
    empty_argv.swap(argv);

    std::vector<std::string> empty_env;
    empty_env.swap(env);

    delete (contiguous_allocator_t*)linear_allocator;
    linear_allocator = nullptr;
}

void process_t::exit(pid_t pid, int exitcode)
{
    scoped_lock lock(processes_lock);

    process_t *process_ptr = lookup(pid);

    thread_t current_thread_id = thread_get_id();

    // Kill all the threads...
    for (thread_t thread : process_ptr->threads) {
        if (thread != current_thread_id) {

        }
        //thread_terminate(thread);

    }

    process_ptr->exitcode = exitcode;
    process_ptr->state = state_t::exited;
    lock.unlock();
    process_ptr->cond.notify_all();

    __longjmp(&process_ptr->exit_jmpbuf, 1);

    //thread_exit(exitcode);
}

bool process_t::add_thread(thread_t tid)
{
    scoped_lock lock(process_lock);
    return threads.push_back(tid);
}

// Returns true when the last thread exits
bool process_t::del_thread(thread_t tid)
{
    scoped_lock lock(process_lock);

    thread_list::iterator it = std::find(threads.begin(), threads.end(), tid);

    if (unlikely(it == threads.end()))
        return false;

    thread_close(tid);

    threads.erase(it);

    return threads.empty();
}

int process_t::wait_for_exit(int pid)
{
    scoped_lock lock(processes_lock);

    process_t *p = lookup(pid);

    if (unlikely(!p))
        return -int(errno_t::EINVAL);

    while (p->state != state_t::exited)
        p->cond.wait(lock);

    return 0;
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
