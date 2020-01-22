#pragma once
#include "vector.h"
#include "cpu/thread_impl.h"
#include "thread.h"
#include "desc_alloc.h"
#include "cpu/thread_impl.h"
#include "desc_alloc.h"
#include "thread.h"
#include "cpu/except_asm.h"
#include "cxxstring.h"

struct fd_table_t
{
    static constexpr ssize_t max_file = 4096;

    desc_alloc_t desc_alloc;

    struct entry_t {
        int16_t id;
        int16_t flags;

        constexpr entry_t()
            : id(0)
            , flags(0)
        {
        }

        constexpr entry_t(uint16_t id)
            : id(id)
            , flags(0)
        {
        }

        constexpr void set(int16_t id, int16_t flags)
        {
            this->id = id;
            this->flags = flags;
        }

        constexpr operator int16_t() const
        {
            return id;
        }

        constexpr entry_t& operator=(int16_t id)
        {
            this->id = id;
            return *this;
        }

        constexpr bool close_on_exec() const
        {
            return flags & 1;
        }

        constexpr void set_close_on_exec(bool close)
        {
            flags = close;
        }
    };

    entry_t ids[max_file];
};

struct auxv_t {
    enum type_t {
        AT_NULL    =  0,    // ignored
        AT_IGNORE  =  1,    // ignored

        // File descriptor of program executable
        AT_EXECFD  =  2,    // a_val

        // The address of the program headers
        AT_PHDR    =  3,    // a_ptr

        // The size of a program header entry
        AT_PHENT   =  4,    // a_val

        // The number of program headers
        AT_PHNUM   =  5,    // a_val

        // The system page size
        AT_PAGESZ  =  6,    // a_val

        // The base address
        AT_BASE    =  7,    // a_ptr

        // Unused
        AT_FLAGS   =  8,    // a_val

        // The entry point
        AT_ENTRY   =  9,    // a_ptr

        AT_NOTELF  = 10,    // a_val

        // The real user id of the main thread
        AT_UID     = 11,    // a_val

        // The effective user id of the main thread
        AT_EUID    = 12,    // a_val

        // The real group id of the main thread
        AT_GID     = 13,    // a_val

        // The effective group id of the main thread
        AT_EGID    = 14     // a_val
    };

    auxv_t()
        : a_type(AT_NULL)
    {
        a_un.a_val = 0;
    }

    auxv_t(type_t type, long val)
        : a_type(type)
    {
        a_un.a_val = val;
    }

    auxv_t(type_t type, void *ptr)
        : a_type(type)
    {
        a_un.a_ptr = ptr;
    }

    auxv_t(type_t type, void (*fnc)())
        : a_type(type)
    {
        a_un.a_fnc = fnc;
    }

    int a_type;
    union {
        long a_val;
        void *a_ptr;
        void (*a_fnc)();
    } a_un;
};

C_ASSERT(sizeof(auxv_t) == sizeof(uintptr_t) * 2);

struct file_mapping_t {
    void *vaddr;
    off_t offset;
    int fd;
};

struct process_t
{
    static constexpr uintptr_t min_addr = 0x400000;
    static constexpr uintptr_t max_addr = 0x7fc000000000;

    process_t() = default;

    bool valid_fd(int fd)
    {
        return fd >= 0 &&
                fd < fd_table_t::max_file &&
                ids.ids[fd] >= 0;
    }

    int fd_to_id(int fd)
    {
        return valid_fd(fd) ? ids.ids[fd].id : -1;
    }

    enum struct state_t {
        unused,
        starting,
        running,
        exited
    };

    std::string path;
    std::vector<std::string> argv;
    std::vector<std::string> env;
    uintptr_t mmu_context = 0;
    void *linear_allocator = nullptr;
    uintptr_t tls_addr = 0;
    size_t tls_msize = 0;
    size_t tls_fsize = 0;
    pid_t pid = 0;
    int uid = 0;
    int gid = 0;
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type process_lock;
    std::condition_variable cond;
    int exitcode = -1;

    __exception_jmp_buf_t exit_jmpbuf = {};

    fd_table_t ids;
    state_t state = state_t::unused;

    static int spawn(pid_t * pid_result,
                     std::string path,
                     std::vector<std::string> argv,
                     std::vector<std::string> env);
    static process_t *init(uintptr_t mmu_context);

    void *get_allocator();
    void set_allocator(void *allocator);

    void destroy();

    _noreturn
    static void exit(pid_t pid, int exitcode);
    bool add_thread(thread_t tid);
    bool del_thread(thread_t tid);

    static int wait_for_exit(int pid);

    int enter_user(uintptr_t ip, uintptr_t sp);

    bool is_main_thread(thread_t tid);

private:
    using thread_list = std::vector<thread_t>;
    thread_list threads;

    std::vector<file_mapping_t> file_maps;

    static process_t *lookup(pid_t pid);

    static process_t *add_locked(scoped_lock const&);
    void remove();
    static process_t *add();
    static int run(void *process_arg);

    int run();
};

__BEGIN_DECLS

void *process_get_allocator();
void process_set_allocator(process_t *process, void *allocator);

__END_DECLS
