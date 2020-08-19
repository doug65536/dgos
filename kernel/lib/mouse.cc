#include "mouse.h"
#include "conio.h"
#include "printk.h"
#include "export.h"
#include "fs/devfs.h"
#include "pipe.h"
#include "cpu/atomic.h"
#include "user_mem.h"

#define DEBUG_MOUSE 0
#if DEBUG_MOUSE
#define MOUSE_TRACE(...) printdbg("mouse: " __VA_ARGS__)
#else
#define MOUSE_TRACE(...) ((void)0)
#endif

class mouse_file_reg_t : public dev_fs_file_reg_t {
public:
    static mouse_file_reg_t *new_registration()
    {
        return new (ext::nothrow) mouse_file_reg_t("mousein");
    }

    mouse_file_reg_t(char const *name)
        : dev_fs_file_reg_t(name)
    {
    }

    bool add_event(mouse_raw_event_t const& event) {
        return pipe.enqueue(&event, sizeof(event), 0);
    }

    struct mouse_file_t : public dev_fs_file_t {
        mouse_file_t(mouse_file_reg_t *owner)
            : owner(owner)
        {
        }

        mouse_file_t(mouse_file_t const&) = delete;
        mouse_file_t &operator=(mouse_file_t const&) = delete;

        // dev_fs_file_t interface
        ssize_t read(char *buf, size_t size, off_t offset) override final
        {
            if (size < sizeof(mouse_raw_event_t))
                return -int(errno_t::EINVAL);

            mouse_raw_event_t ev;

            ssize_t dequeue_sz = owner->pipe.dequeue(
                        &ev, sizeof(ev), INT64_MAX);

            if (likely(dequeue_sz == sizeof(ev))) {
                if (unlikely(!mm_copy_user(buf, &ev, sizeof(ev))))
                    return -int(errno_t::EFAULT);

                return sizeof(ev);
            }

            return 0;
        }

        // dev_fs_file_t interface
        ssize_t write(char const *buf, size_t size,
                      off_t offset) override final
        {
            return -int(errno_t::EROFS);
        }

        ino_t get_inode() const override final
        {
            return -int(errno_t::ENOSYS);
        }

        mouse_file_reg_t *owner;
    };

    // dev_fs_file_reg_t interface
    dev_fs_file_t *open(int flags, mode_t mode) override
    {
        return new (ext::nothrow) mouse_file_t(this);
    }

    pipe_t pipe;
};

static mouse_file_reg_t *mouse_file;

// Prepare to receive mouse_event calls
mouse_file_reg_t *mouse_file_instance()
{
    mouse_file_reg_t *old_mouse_file = atomic_ld_acq(&mouse_file);

    if (likely(old_mouse_file))
        return old_mouse_file;

    // A stampede of threads could get here and construct a
    // mouse_file_reg_t, but only one will win the first cmpxchg,
    // and everyone else fail their cmpxchg, and use winner's object
    mouse_file_reg_t *new_mouse_file = new (ext::nothrow)
            mouse_file_reg_t("mousein");

    if (unlikely(!new_mouse_file))
        panic_oom();

    if (unlikely(!new_mouse_file->pipe.reserve(16)))
        panic_oom();

    // Try to win race
    old_mouse_file = atomic_cmpxchg(&mouse_file, nullptr, new_mouse_file);

    // If won race, done
    if (likely(old_mouse_file == nullptr)) {
        devfs_register(new_mouse_file);
        return new_mouse_file;
    }

    // Lost race
    delete new_mouse_file;

    return old_mouse_file;
}

EXPORT void mouse_event(mouse_raw_event_t event)
{
    MOUSE_TRACE("hdist=%+d, vdist=%+d, buttons=0x%x, wheel=%+d\n",
                event.hdist, event.vdist, event.buttons, event.wdist);

    mouse_file_instance()->add_event(event);
}

EXPORT void mouse_file_init()
{
    mouse_file_instance();
}
