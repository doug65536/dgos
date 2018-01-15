#include <spawn.h>
#include <string.h>
#include <new.h>

struct __posix_spawn_file_action_t {
    enum struct action_t {
        invalid,
        open,
        dup2,
        close
    };

    struct open_action_t {
        int fd;
        char *restrict path;
        int oflag;
        mode_t mode;

        open_action_t(int fd, char const * restrict path,
                      int oflag, mode_t mode)
            : fd(fd)
            , path(strdup(path))
            , oflag(oflag)
            , mode(mode)
        {
        }

        ~open_action_t()
        {
            free(path);
        }
    };

    struct dup2_action_t {
        int from;
        int to;

        dup2_action_t(int from, int to)
            : from(from), to(to)
        {
        }
    };

    struct close_action_t {
        int fd;

        close_action_t(int fd)
            : fd(fd)
        {
        }
    };

    union action_entry_t {
        open_action_t open_action;
        dup2_action_t dup2_action;
        close_action_t close_action;

        action_entry_t()
        {
        }

        ~action_entry_t()
        {
        }
    };

    __posix_spawn_file_action_t& operator=(open_action_t&& item) {
        new ((void*)&entry.open_action) open_action_t(item);
        return *this;
    }

    __posix_spawn_file_action_t& operator=(dup2_action_t&& item) {
        new (&entry.dup2_action) dup2_action_t(item);
        return *this;
    }

    __posix_spawn_file_action_t& operator=(close_action_t&& item) {
        new (&entry.close_action) close_action_t(item);
        return *this;
    }

    ~__posix_spawn_file_action_t()
    {
        destruct();
    }

    void destruct() {
        switch (action) {
        case action_t::open:
            entry.open_action.~open_action_t();
            break;

        case action_t::dup2:
            entry.dup2_action.~dup2_action_t();
            break;

        case action_t::close:
            entry.close_action.~close_action_t();
            break;

        case action_t::invalid:
            break;
        }

        action = action_t::invalid;
    }

    action_t action;
    action_entry_t entry;
};
