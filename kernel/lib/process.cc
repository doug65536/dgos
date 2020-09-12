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

static ext::vector<process_ptr_t> processes;
static size_t process_count;
using processes_lock_type = process_t::lock_type;
using processes_scoped_lock = ext::unique_lock<processes_lock_type>;
static processes_lock_type processes_lock;
static pid_t process_first_free;
static pid_t process_last_free;

process_t *process_t::add_locked(processes_scoped_lock const&)
{
    pid_t pid;
    size_t realloc_count = 0;

    process_t *process = new (ext::nothrow) process_t();
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
                     ext::string path,
                     ext::vector<ext::string> argv,
                     ext::vector<ext::string> env)
{
    *pid_result = -1;

    process_t *process = process_t::add();

    processes_scoped_lock lock(process->process_lock);

    process->path = ext::move(path);
    process->argv = ext::move(argv);
    process->env = ext::move(env);

    // Return the assigned PID
    *pid_result = process->pid;

    if (unlikely(!process->add_thread(-1, lock)))
        return -int(errno_t::ENOMEM);

    if (unlikely(thread_create(&process->threads.back(),
                               &process_t::run, process,
                               "user-process", 0, true, true) < 0))
        return -int(errno_t::EAGAIN);

    // Wait for it to finish starting
    while (process->state == process_t::state_t::starting)
        process->cond.wait(lock);

    return (process->state == process_t::state_t::running ||
            process->state == process_t::state_t::exited)
            ? 0
            : -int(errno_t::EFAULT);
}

intptr_t process_t::run(void *process_arg)
{
    return ((process_t*)process_arg)->run();
}

// This is the thread start function for the new kernel thread backing
// this user thread
// Runs in the context of a new thread
intptr_t process_t::start_clone_thunk(void *clone_data)
{
    clone_data_t *cdp = (clone_data_t*)clone_data;

    // Copy it to the stack
    clone_data_t data = *cdp;

    // run_clone doesn't return, have to do this first
    delete cdp;

    // goto the usermode entry
    return data.process->start_clone(data);
}

// Runs in the context of a new thread
intptr_t process_t::start_clone(clone_data_t const& data)
{
    uintptr_t tid = thread_get_id();

    // Volatile because we expect it to behave well after setjmp returns again
    scoped_lock volatile lock(processes_lock);

    create_tls();

    intptr_t index = thread_index(tid, lock);

    assert_msg(index >= 0, "Can't find new thread's exit jmpbuf");

    __exception_jmp_buf_t *buf = user_threads[index].jmpbuf;

    if (!__setjmp(buf)) {
        void *buf_rsp = buf->rsp;
        lock.unlock();

        arch_jump_to_user(uintptr_t((void*)data.bootstrap), uintptr_t(data.sp),
                   uintptr_t(buf_rsp), use64,
                   tid, uintptr_t(data.fn), uintptr_t(data.arg));
    }

    // Acquire lock again
    lock.lock();

    // It might have moved since releasing the lock
    index = thread_index(tid, lock);

    intptr_t exitcode = user_threads[index].exitcode;

    user_threads.erase(user_threads.begin() + index);
    threads.erase(threads.begin() + index);

    return exitcode;
}

void process_t::exit_thread(thread_t tid, void *exitcode)
{
    scoped_lock lock(process_lock);

    intptr_t index = thread_index(tid, lock);

    if (likely(index >= 0)) {
        user_thread_info_t &info = user_threads[index];

        info.exitcode = intptr_t(exitcode);

        // Detach the thread if not detached already
        if (!info.detached) {
            info.detached = true;
            thread_close(tid);
        }

        __exception_jmp_buf_t *jmpbuf = info.jmpbuf;

        lock.unlock();

        __longjmp(jmpbuf, 1);
    }

    panic("Thread %d not found in exit_thread!", tid);
}


int process_t::clone(void (*bootstrap)(int tid, void *(*fn)(void *), void *arg),
                     void *child_stack, int flags,
                     void *(*fn)(void *arg), void *arg)
{
    clone_data_t *kernel_thread_arg = new (ext::nothrow) clone_data_t();

    kernel_thread_arg->process = this;
    kernel_thread_arg->sp = child_stack;
    kernel_thread_arg->bootstrap = bootstrap;
    kernel_thread_arg->fn = fn;
    kernel_thread_arg->arg = arg;

    if (unlikely(!kernel_thread_arg))
        return -int(errno_t::ENOMEM);

#ifdef __x86_64__
    // Intel ineptly throws #GP in kernel mode with user's stack if
    // user rip is not canonical. Check.
    if (unlikely((intptr_t(uintptr_t(kernel_thread_arg->bootstrap)
                           << 16) >> 16) !=
                 intptr_t(kernel_thread_arg->bootstrap)))
        return -int(errno_t::EFAULT);
#endif

    scoped_lock lock(processes_lock);

    // Create the user_thread_info_t object
    if (unlikely(!user_threads.emplace_back()))
        return -1;

    if (unlikely(!threads.push_back(-1))) {
        panic_oom();
        user_threads.pop_back();
        return -1;
    }

    if (thread_create(&threads.back(),
                      &process_t::start_clone_thunk, kernel_thread_arg,
                      "user_thread", 0, true, true) < 0)
        return -1;

    if (flags & CLONE_FLAGS_DETACHED)
        thread_close(threads.back());

    return threads.back();
}

int process_t::kill(int pid, int sig)
{
    if (unlikely(pid < 0))
        return -int(errno_t::ESRCH);

    if (pid >= (int)processes.size())
        return -int(errno_t::ESRCH);

    if (unlikely(sig < 0))
        return -int(errno_t::EINVAL);

    if (unlikely(sig >= 32))
        return -int(errno_t::EINVAL);

    process_t *p = processes[pid].p;

    if (unlikely(!p))
        return -int(errno_t::ESRCH);

    return p->send_signal(sig);
}

int process_t::send_signal(int sig)
{
    return -int(errno_t::ENOSYS);
}

int process_t::send_signal_to_self(int sig)
{
    return -int(errno_t::ENOSYS);
}

// Hack to reuse module loader symbol auto-load hook for processes too
extern void modload_load_symbols(char const *path, uintptr_t text_addr,
                                 intptr_t base_addr);

template<typename P>
intptr_t process_t::load_elf_with_policy(int fd)
{
    using Ehdr = typename P::Ehdr;
    using Phdr = typename P::Phdr;

    Ehdr hdr;

    ssize_t read_size;

    read_size = file_pread(fd, &hdr, sizeof(hdr), 0);
    if (unlikely(read_size != sizeof(hdr))) {
        printdbg("Failed to read ELF header\n");
        return int(errno_t(int(read_size)));
    }

    // Allocate memory for program headers
    ext::vector<Phdr> program_hdrs;
    if (unlikely(!program_hdrs.resize(hdr.e_phnum))) {
        printdbg("Failed to allocate memory for program headers\n");
        return -int(errno_t::ENOMEM);
    }

    // Read program headers
    read_size = sizeof(Phdr) * hdr.e_phnum;
    if (unlikely(read_size != file_pread(
                     fd,
                     program_hdrs.data(),
                     read_size,
                     hdr.e_phoff))) {
        printdbg("Failed to read program headers\n");
        return int(errno_t(int(read_size)));
    }

    size_t last_region_st = ~size_t(0);
    size_t last_region_en = 0;
    size_t last_region_sz;

    uintptr_t first_exec = UINTPTR_MAX;

    // Map every section first, just in case any pages overlap
    for (Phdr const& ph : program_hdrs) {
        // If it is not loaded, ignore
        if (unlikely(ph.p_type != PT_LOAD))
            continue;

        // If it is not readable, writable or executable, ignore
        if (unlikely((ph.p_flags & (PF_R | PF_W | PF_X)) == 0))
            continue;

        // No memory size? Ignore
        if (unlikely(ph.p_memsz == 0))
            continue;

        // See if it begins in reserved space
        if (unlikely(!mm_is_user_range((void*)(uintptr_t)ph.p_vaddr,
                                       ph.p_memsz))) {
            printdbg("The virtual address is not in user address space\n");
            return -int(errno_t::EFAULT);
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
        if (unlikely(ph.p_vaddr >= last_region_st &&
                     ph.p_vaddr + ph.p_memsz <= last_region_en))
            continue;

        // Update region reserved by last mapping
        last_region_st = ph.p_vaddr & -PAGESIZE;
        last_region_en = ((ph.p_vaddr + ph.p_memsz) + PAGESIZE - 1) & -PAGESIZE;
        last_region_sz = last_region_en - last_region_st;

        if (unlikely(last_region_sz &&
                     mmap((void*)last_region_st, last_region_sz, page_prot,
                          MAP_USER | MAP_NOCOMMIT) == MAP_FAILED)) {
            printdbg("Failed to reserve %#" PRIx64
                     " bytes of address space"
                     " with protection %d"
                     " at %#" PRIx64 "! \n",
                     uint64_t(ph.p_memsz),
                     page_prot, uint64_t(ph.p_vaddr));
            return -int(errno_t::ENOMEM);
        }
    }

    // Read everything after mapping the memory
    for (Phdr const& ph : program_hdrs) {
        // If it is not loaded, ignore
        if (unlikely(ph.p_type != PT_LOAD))
            continue;

        if (unlikely(!mm_is_user_range((void*)(intptr_t)ph.p_vaddr,
                                       ph.p_memsz)))
            return int(errno_t::EFAULT);

        read_size = ph.p_filesz;
        if (likely(ph.p_filesz > 0)) {
            if (unlikely(read_size != file_pread(
                             fd, (void*)uintptr_t(ph.p_vaddr),
                             read_size, ph.p_offset))) {
                printdbg("Failed to read program headers!\n");
                return -int(errno_t::ENOEXEC);
            }
        }
    }

    // Make read only pages read only
    for (Phdr const& ph : program_hdrs) {
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

        if (unlikely(mprotect((void*)uintptr_t(ph.p_vaddr),
                              ph.p_memsz, page_prot) < 0)) {
            printdbg("Failed to set page protection\n");
            return -1;
        }
    }

    // Find TLS
    for (Phdr const& ph : program_hdrs) {
        // If it is a TLS header, remember the range
        if (unlikely(ph.p_type == PT_TLS)) {
            if (unlikely(tls_addr != 0))
                printdbg("Strange, multiple TLS program headers!");

            tls_addr = ph.p_vaddr;
            tls_msize = ph.p_memsz;
            tls_fsize = ph.p_filesz;
        }
    }

    return first_exec;
}

// Must run in the context of the new thread
void *process_t::create_tls()
{
    // The space for the uintptr_t is for the pointer to itself at TLS offset 0
    size_t tls_vsize = PAGE_SIZE + sizeof(uintptr_t) + tls_msize + PAGE_SIZE;

    void *tls = mmap(nullptr, tls_vsize, PROT_NONE, MAP_USER);

    if (tls == MAP_FAILED)
        return nullptr;

    // 4KB guard region around TLS
    uintptr_t tls_area = uintptr_t(tls) + PAGESIZE;

    if (unlikely(!mm_is_user_range((void*)tls_area,
                                   sizeof(uintptr_t) + tls_msize)))
        return nullptr;

    if (unlikely(mprotect((void*)tls_area, tls_msize + sizeof(uintptr_t),
                          PROT_READ | PROT_WRITE) < 0))
        return nullptr;

    // Explicitly commit faster than taking demand faults
    if (unlikely(madvise((void*)tls_area, tls_msize + sizeof(uintptr_t),
                         MADV_WILLNEED) < 0))
        return nullptr;

    // Copy template into TLS area
    if (unlikely(!mm_copy_user((char*)tls_area, (void*)tls_addr, tls_fsize)))
        return nullptr;

    // Zero fill the region not specified in the file
    if (unlikely(!mm_copy_user((char*)tls_area + tls_fsize, nullptr,
                               tls_msize - tls_fsize)))
        return nullptr;

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
        return nullptr;

    // Point appropriate tls selector register at it
    if (use64)
        thread_set_fsbase(-1, tls_ptr);
    else
        thread_set_gsbase(-1, tls_ptr);

    return (void*)tls_ptr;
}

int process_t::detach(int tid)
{
    scoped_lock lock(process_lock);

    intptr_t index = thread_index(tid, lock);

    if (unlikely(index < 0))
        return -int(errno_t::ESRCH);

    if (unlikely(user_threads[index].detached))
        return -int(errno_t::EINVAL);

    thread_close(tid);

    user_threads[index].detached = true;

    return 0;
}

int process_t::is_joinable(int tid)
{
    scoped_lock lock(process_lock);

    intptr_t index = thread_index(tid, lock);

    if (unlikely(index < 0))
        return -int(errno_t::ESRCH);

    return !user_threads[index].detached;
}

// Returns when program exits
intptr_t process_t::run()
{
    // Attach this kernel thread to this process
    thread_set_process(-1, this);

    // Simply load it for now
    Elf64_Ehdr hdr;

    // Open a stdin, stdout, and stderr
    int fd_i = file_openat(AT_FDCWD, "/dev/conin", 0, 0);
    int fd_o = file_openat(AT_FDCWD, "/dev/conout", 0, 0);
    //int fd_e = file_openat(AT_FDCWD, "/dev/conout", 0, 0);

    ids.desc_alloc.take({0, 1, 2});
    ids.ids[0].set(fd_i, 0);
    ids.ids[1].set(fd_o, 0);
    //ids.ids[2].set(fd_e, 0);

    assert(fd_i == 0);
    assert(fd_o == 1);
//    assert(fd_e == 2);

    file_t fd{file_openat(AT_FDCWD, path.c_str(), O_RDONLY)};

    if (unlikely(fd < 0)) {
        printdbg("Failed to open executable %s\n", path.c_str());
        return int(errno_t(int(fd)));
    }

    ssize_t read_size;

    read_size = file_read(fd, &hdr, sizeof(hdr));
    if (unlikely(read_size != sizeof(hdr))) {
        printdbg("Failed to read ELF header\n");
        return int(errno_t(int(read_size)));
    }

    intptr_t first_exec;

    use64 = (hdr.e_machine == EM_AMD64);

    // Switch to user address space
    mmu_context = mm_new_process(this, use64);
    if (unlikely(!mmu_context))
        return -int(errno_t::ENOMEM);

    if (use64)
        first_exec = load_elf_with_policy<Elf64_Policy>(fd);
    else
        first_exec = load_elf_with_policy<Elf32_Policy>(fd);

    if (unlikely(first_exec < 0))
        return first_exec;

    char const *path_ptr = path.c_str();
    char const *filename = strrchr(path_ptr, '/');
    filename = filename ? filename + 1 : path_ptr;

    modload_load_symbols(filename, first_exec, 0);

    // Initialize the stack

    // 8MB
    size_t stack_size = 8 << 20;
    char *stack_memory = (char*)mmap(
                nullptr, stack_size, PROT_READ | PROT_WRITE,
                MAP_STACK | MAP_NOCOMMIT | MAP_USER);

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
    void *tls = create_tls();

    if (unlikely(!tls))
        panic_oom();

    static_assert(sizeof(uintptr_t) == sizeof(void*), "Unexpected size");


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
    if (unlikely(!mm_copy_user(stack_ptr, &argc, sizeof(argc))))
        return -1;

    ext::vector<auxv_t> auxent;

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

    // Add new jmpbuf to find kernel stack

    if (unlikely(!user_threads.emplace_back()))
        return -1;

    __exception_jmp_buf_t *buf = user_threads.back().jmpbuf;

    state = state_t::running;
    lock.unlock();
    cond.notify_all();

    return enter_user(hdr.e_entry, uintptr_t(stack_ptr), use64, buf);
}

int process_t::enter_user(uintptr_t ip, uintptr_t sp, bool use64,
                          __exception_jmp_buf_t *buf)
{
    // _Exit syscall will longjmp here
    if (!__setjmp(buf)) {
        // When interrupts occur, use the stack space we have here
        // isr_sysret does not return
        printdbg("Entering user process\n");
        arch_jump_to_user(ip, sp, uint64_t(buf->rsp) & -16, use64,
                   thread_get_id(), 0, 0);
    }

    // Execution reaches here when a thread of the process calls exit

    for (thread_list::const_reverse_iterator it = threads.crbegin(),
         en = threads.crend(); it != en; ++it) {
        thread_t tid = *it;
        del_thread(tid);
    }

    // exiting program continues here
    return exitcode;
}

bool process_t::is_main_thread(thread_t tid)
{
    return threads.front() == tid;
}

void *process_t::get_allocator()
{
    return linear_allocator;
}

void process_t::set_allocator(void *allocator)
{
    assert(linear_allocator == nullptr);
    linear_allocator = allocator;
}

void process_t::destroy()
{
    ext::string empty_path;
    empty_path.swap(path);

    ext::vector<ext::string> empty_argv;
    empty_argv.swap(argv);

    ext::vector<ext::string> empty_env;
    empty_env.swap(env);

    delete (contiguous_allocator_t*)linear_allocator;
    linear_allocator = nullptr;
}

void process_t::exit(pid_t pid, intptr_t exitcode)
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

    intptr_t index = process_ptr->thread_index(current_thread_id, lock);

    assert_msg(index >= 0, "Thread exit jmpbuf not found in process_t::exit");

    __exception_jmp_buf_t *exit_buf = index >= 0
            ? process_ptr->user_threads[index].jmpbuf.get()
            : nullptr;

    lock.unlock();
    process_ptr->cond.notify_all();

    if (likely(exit_buf))
        __longjmp(exit_buf, 1);

    panic("Thread has no exit jmpbuf!");
}

bool process_t::add_thread(thread_t tid, scoped_lock &lock)
{
    return threads.push_back(tid);
}

// Returns true when the last thread exits
bool process_t::del_thread(thread_t tid)
{
    scoped_lock lock(process_lock);

    thread_list::iterator it = ext::find(threads.begin(), threads.end(), tid);

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

__exception_jmp_buf_t *process_t::exit_jmpbuf(int tid, scoped_lock& lock)
{
    intptr_t i = thread_index(tid, lock);

    if (unlikely(i < 0))
        return nullptr;

    return user_threads[i].jmpbuf.get();
}

__exception_jmp_buf_t *process_get_exit_jmpbuf(
        int tid, process_t::scoped_lock &lock)
{
    process_t *process = thread_current_process();
    return process->exit_jmpbuf(tid, lock);
}

void process_set_restorer(process_t *p, __sig_restorer_t value)
{
    p->sigrestorer = value;
}

__sig_restorer_t process_get_restorer(process_t *p)
{
    return p->sigrestorer;
}
