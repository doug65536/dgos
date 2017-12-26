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

        entry_t()
            : id(0)
            , flags(0)
        {
        }

        entry_t(uint16_t id)
            : id(id)
            , flags(0)
        {
        }

        void set(int16_t id, int16_t flags)
        {
            this->id = id;
            this->flags = flags;
        }

        operator int16_t() const
        {
            return id;
        }

        entry_t& operator=(int16_t id)
        {
            this->id = id;
            return *this;
        }

        bool close_on_exec() const
        {
            return flags & 1;
        }

        void set_close_on_exec(bool close)
        {
            flags = close;
        }
    };

    entry_t ids[max_file];
};

struct process_t
{
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
    char **args;
    char **env;
    uintptr_t mmu_context;
    void *linear_allocator;
    pid_t pid;
    spinlock process_lock;
    condition_variable cond;
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

    static void exit(pid_t pid, int exitcode);
    static bool add_thread(pid_t pid, thread_t tid);

private:
    vector<thread_t> threads;

    static process_t *lookup(pid_t pid);

    static process_t *add_locked(unique_lock<spinlock> const&);
    void remove();
    static process_t *add();
    static int start(void *process_arg);
    int start();
};

void *process_get_allocator();
void process_set_allocator(process_t *process, void *allocator);
