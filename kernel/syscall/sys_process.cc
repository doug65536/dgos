#include "sys_process.h"
#include "process.h"
#include "printk.h"
#include "hash_table.h"
#include "threadsync.h"
#include "chrono.h"
#include "user_mem.h"
#include "syscall/sys_limits.h"

struct futex_tab_ent_t {
    uintptr_t addr;
    std::condition_variable wake;
};

using futex_tab_t = hashtbl_t<
    futex_tab_ent_t,
    uintptr_t, &futex_tab_ent_t::addr
>;

using lock_type = std::mutex;
using scoped_lock = std::unique_lock<lock_type>;
static lock_type futex_lock;
static futex_tab_t futex_tab;

void sys_exit(int exitcode)
{

    process_t::exit(-1, exitcode);
}


#define FUTEX_PRIVATE_FLAG  0x80000000
#define FUTEX_WAIT          0x00000001
#define FUTEX_WAKE          0x00000002

static long futex_wait(int *uaddr, int expect, int64_t timeout_time)
{
    scoped_lock lock(futex_lock);

    // Check value inside lock in case a wake raced ahead of us just
    // early enough to miss it, we won't miss the memory change
    int value = 0;
    if (!mm_copy_user(&value, uaddr, sizeof(value)))
        return -int(errno_t::EFAULT);

    if (value != expect)
        return -int(errno_t::EAGAIN);

    futex_tab_ent_t *fent = futex_tab.lookup(&value);

    if (!fent) {
        std::unique_ptr<futex_tab_ent_t> new_ent(
                    new (std::nothrow) futex_tab_ent_t);

        if (unlikely(!new_ent))
            return -int(errno_t::ENOMEM);

        new_ent->addr = mphysaddr(uaddr);
        if (likely(futex_tab.insert(new_ent))) {
            fent = new_ent.release();
        } else {
            fent = nullptr;
            assert(!"Not possible!");
        }
    }

    if (fent) {
        if (timeout_time == 0)
            fent->wake.wait(lock);
        else
            fent->wake.wait_until(
                        lock,
                        std::chrono::time_point<std::chrono::steady_clock>(
                            std::chrono::nanoseconds(timeout_time)));
    }

    return 0;
}

static long futex_wake(int *uaddr, int max_awakened)
{
    scoped_lock lock(futex_lock);

    uintptr_t addr = mphysaddr(uaddr);

    futex_tab_ent_t *fent = futex_tab.lookup(&addr);

    if (fent) {
        if (max_awakened == INT_MAX)
            fent->wake.notify_all();
        else
            fent->wake.notify_n(max_awakened);
    }

    return 0;
}

static int64_t timeout_from_timespec(struct timespec const *t)
{
    return t ? t->tv_sec * 1000000000 + t->tv_nsec : INT64_MAX;
}

long sys_futex(int *uaddr, int futex_op, int val,
               struct timespec const *timeout, int *uaddr2, int val3)
{
    switch (futex_op) {
    case FUTEX_WAIT:
        return futex_wait(uaddr, val, timeout_from_timespec(timeout));
    case FUTEX_WAKE:
        return futex_wake(uaddr, val);
    default:
        return -int(errno_t::EINVAL);
    }
}

long sys_posix_spawn(pid_t *restrict pid,
                     char const *restrict path,
                     posix_spawn_file_actions_t const *file_actions,
                     posix_spawnattr_t const *restrict attr,
                     char const * * restrict argv,
                     char const * * restrict envp)
{
    mm_copy_string_result_t path_string = mm_copy_user_string(path, PATH_MAX);
    if (!path_string.second)
        return -int(errno_t::EFAULT);

    std::vector<std::string> argv_items;
    std::vector<std::string> envp_items;
    std::vector<std::string> *curr_items = &argv_items;
    char const ** restrict cur_src = argv;

    for (size_t pass = 0; pass < 2; ++pass) {
        char const *str_entry = nullptr;
        do {
            if (unlikely(curr_items->size() >= ARG_MAX))
                return -int(errno_t::EINVAL);

            str_entry = nullptr;
            if (unlikely(!mm_copy_user(&str_entry, cur_src +
                                       curr_items->size(),
                                       sizeof(str_entry)))) {
                return -int(errno_t::EFAULT);
            }

            if (str_entry == nullptr)
                break;

            mm_copy_string_result_t str = mm_copy_user_string(
                        str_entry, PATH_MAX);

            if (unlikely(!str.second))
                return -int(errno_t::EINVAL);

            if (unlikely(!curr_items->emplace_back(std::move(str.first))))
                return -int(errno_t::ENOMEM);
        } while (str_entry);

        curr_items = &envp_items;
        cur_src = envp;
    }

    pid_t pid_result = 0;
    long result = process_t::spawn(&pid_result,
                                   std::move(path_string.first),
                                   std::move(argv_items),
                                   std::move(envp_items));

    if (!mm_copy_user(pid, &pid_result, sizeof(*pid)))
        return -int(errno_t::EFAULT);

    return result;
}
