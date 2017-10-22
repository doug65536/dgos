#pragma once
#include "cpu/thread_impl.h"
#include "thread.h"
#include "desc_alloc.h"
#include "cpu/thread_impl.h"
#include "desc_alloc.h"

typedef int pid_t;

struct fd_table_t
{
    static constexpr ssize_t max_file = 4096;
    
    desc_alloc_t desc_alloc;
    
    struct entry_t {
        int16_t id;
        int16_t flags;
        
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
    
    pid_t pid;
    char *path;
    char **args;
    char **env;
    uintptr_t mmu_context;
    void *linear_allocator;
    
    fd_table_t ids;
};

process_t *process_init(uintptr_t mmu_context);

// Load and execute the specified program
int process_spawn(pid_t * pid,
                  char const * path,
                  char const * const * argv,
                  char const * const * envp);

void process_remove(process_t *process);

void *process_get_allocator();
void process_set_allocator(void *allocator);
