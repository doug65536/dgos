#pragma once
#include "vector.h"
#include "cpu/thread_impl.h"
#include "thread.h"
#include "desc_alloc.h"
#include "cpu/thread_impl.h"
#include "desc_alloc.h"
#include "thread.h"

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
        AT_EXECFD  =  2,    // a_val
        AT_PHDR    =  3,    // a_ptr
        AT_PHENT   =  4,    // a_val
        AT_PHNUM   =  5,    // a_val
        AT_PAGESZ  =  6,    // a_val
        AT_BASE    =  7,    // a_ptr
        AT_FLAGS   =  8,    // a_val
        AT_ENTRY   =  9,    // a_ptr
        AT_NOTELF  = 10,    // a_val
        AT_UID     = 11,    // a_val
        AT_EUID    = 12,    // a_val
        AT_GID     = 13,    // a_val
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

struct process_t
{
    process_t()
        : path(nullptr)
        , argv(nullptr)
        , env(nullptr)
        , mmu_context(0)
        , linear_allocator(nullptr)
        , pid(0)
        , exitcode(0)
        , state(state_t::unused)
    {
    }

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

    char *path;
    char **argv;
    char **env;
    size_t argc;
    size_t envc;
    uintptr_t mmu_context;
    void *linear_allocator;
    pid_t pid;
    using lock_type = std::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type process_lock;
    std::condition_variable cond;
    int exitcode;

    fd_table_t ids;
    state_t state;

    static int spawn(pid_t * pid_result,
                     char const * path,
                     char const * const * argv,
                     char const * const * envp);
    static process_t *init(uintptr_t mmu_context);

    void *get_allocator();
    void set_allocator(void *allocator);

    void destroy();

    static void exit(pid_t pid, int exitcode);
    bool add_thread(thread_t tid);
    bool del_thread(thread_t tid);

private:
    using thread_list = std::vector<thread_t>;
    thread_list threads;

    static process_t *lookup(pid_t pid);

    static process_t *add_locked(scoped_lock const&);
    void remove();
    static process_t *add();
    static int start(void *process_arg);
    int start();
};

void *process_get_allocator();
void process_set_allocator(process_t *process, void *allocator);
