#include "sys_process.h"
#include "process.h"
#include "printk.h"
#include "hash_table.h"
#include "threadsync.h"
#include "chrono.h"
#include "user_mem.h"
#include "syscall/sys_limits.h"
#include "thread.h"
#include "user_mem.h"

struct futex_tab_ent_t {
    uintptr_t addr = 0;
    size_t waiter_count = 0;
    std::condition_variable wake;
};

using futex_tab_t = hashtbl_t<
    futex_tab_ent_t,
    uintptr_t, &futex_tab_ent_t::addr>;

using lock_type = std::mutex;
using scoped_lock = std::unique_lock<lock_type>;
static lock_type futex_lock;
static futex_tab_t futex_tab;

#define FUTEX_PRIVATE_FLAG  0x80000000
#define FUTEX_WAIT          0x00000001
#define FUTEX_WAKE          0x00000002
#define FUTEX_WAKE_OP       0x00000003
#define FUTEX_WAIT_OP       0x00000004

#define FUTEX_OP_SET    0  /* uaddr2 = oparg; */
#define FUTEX_OP_ADD    1  /* uaddr2 += oparg; */
#define FUTEX_OP_OR     2  /* uaddr2 |= oparg; */
#define FUTEX_OP_ANDN   3  /* uaddr2 &= ~oparg; */
#define FUTEX_OP_XOR    4  /* uaddr2 ^= oparg; */
#define FUTEX_OP_ARG_SHIFT    8  /* operand = 1 << oparg */

#define FUTEX_CMP_EQ    0
#define FUTEX_CMP_NE    1
#define FUTEX_CMP_LT    2
#define FUTEX_CMP_LE    3
#define FUTEX_CMP_GT    4
#define FUTEX_CMP_GE    5

#define FUTEX_OP(op, oparg, cmp, cmparg) \
    ((((op) & 0xf) << 28) | \
    (((cmp) & 0xf) << 24) | \
    (((oparg) & 0xfff) << 12) | \
    ((cmparg) & 0xfff))

static long futex_wait(int *uptr, int expect, uint64_t timeout_time)
{
    scoped_lock lock(futex_lock);

    uintptr_t uphysaddr = mphysaddr(uptr);

    // Check value inside lock in case a wake raced ahead of us just
    // early enough to miss it, we won't miss the memory change
    int value = 0;
    if (unlikely(!mm_copy_user(&value, uptr, sizeof(value))))
        return -int(errno_t::EFAULT);

    if (unlikely(value != expect))
        return -int(errno_t::EAGAIN);

    futex_tab_ent_t *fent = futex_tab.lookup(&uphysaddr);

    std::unique_ptr<futex_tab_ent_t> new_ent;

    long status = 0;

    if (!fent) {
        // Create a new one

        if (unlikely(!new_ent.reset(new (ext::nothrow) futex_tab_ent_t())))
            return -int(errno_t::ENOMEM);

        new_ent->addr = uphysaddr;

        if (unlikely(!futex_tab.insert(new_ent)))
            return -int(errno_t::ENOMEM);

        fent = new_ent.release();
    }

    if (fent) {
        ++fent->waiter_count;

        if (timeout_time == 0) {
            fent->wake.wait(lock);
        } else {
            std::chrono::steady_clock::time_point const
                    timeout{std::chrono::nanoseconds(timeout_time)};
            if (unlikely(fent->wake.wait_until(lock, timeout) ==
                         std::cv_status::timeout))
                status = -int(errno_t::ETIMEDOUT);
        }

        // When the last waiter stops waiting, delete the
        // hash table entry and object
        if (--fent->waiter_count == 0) {
            futex_tab.del(&uptr);
            delete fent;
        }
    }

    return status;
}

static long futex_wake(int *uaddr, int max_awakened)
{
    scoped_lock lock(futex_lock);

    uintptr_t addr = mphysaddr(uaddr);

    // Lookup waiter record
    futex_tab_ent_t *fent = futex_tab.lookup(&addr);

    if (fent) {
        // Found waiter(s)

        if (max_awakened == INT_MAX)
            fent->wake.notify_all();
        else
            fent->wake.notify_n(max_awakened);
    }

    // Nobody is waiting, that's fine. Continue.

    return 0;
}

static int futex_apply_op(int value, int op, int oparg)
{
    if (op & FUTEX_OP_ARG_SHIFT) {
        op -= FUTEX_OP_ARG_SHIFT;
        oparg = int(1U << oparg);
    }

    switch (op) {
    case FUTEX_OP_SET:
        return oparg;

    case FUTEX_OP_ADD:
        return value + oparg;

    case FUTEX_OP_OR:
        return value | oparg;

    case FUTEX_OP_ANDN:
        return value & ~oparg;

    case FUTEX_OP_XOR:
        return value ^ oparg;

    default:
        return value;

    }
}

static int futex_apply_cmp(int value, int cmp, int cmparg)
{
    switch (cmp) {
    case FUTEX_CMP_EQ: return value == cmparg;
    case FUTEX_CMP_NE: return value != cmparg;
    case FUTEX_CMP_LT: return value < cmparg;
    case FUTEX_CMP_LE: return value <= cmparg;
    case FUTEX_CMP_GT: return value > cmparg;
    case FUTEX_CMP_GE: return value >= cmparg;
    default: return false;
    }
}

struct op_param_t {
    explicit op_param_t(int n) noexcept
        : n(n)
    {
    }

    int arg() const noexcept
    {
        return n & 0xfff;
    }

    int oparg() const noexcept
    {
        // Sign extend
        return int(unsigned(n) << (sizeof(int) * CHAR_BIT - 24)) >>
                        (sizeof(int) * CHAR_BIT - 12);
    }

    int cmp() const noexcept
    {
        return (n >> 24) & 0xf;
    }

    int op() const noexcept
    {
        return (n >> 28) & 0xf;
    }

    int n;
};

static long futex_wake_op_locked(int *uaddr2, int op_param,
                                 int *uaddr, int wake, int wake2,
                                 scoped_lock &lock);

static long futex_wake_op(int *uaddr2, int op_param,
                          int *uaddr, int wake, int wake2)
{
    scoped_lock lock(futex_lock);
    return futex_wake_op_locked(uaddr2, op_param, uaddr, wake, wake2, lock);
}

static long futex_wake_op_locked(int *uaddr2, int op_param,
                                 int *uaddr, int wake, int wake2,
                                 scoped_lock& lock)
{
    uintptr_t addr = mphysaddr(uaddr);
    uintptr_t addr2 = mphysaddr(uaddr);

    op_param_t opp{op_param};
    int cmparg = opp.arg();
    int oparg = opp.oparg();
    int cmp = opp.cmp();
    int op = opp.op();

    int oldval;
    if (unlikely(!mm_copy_user(&oldval, uaddr2, sizeof(oldval))))
        return -int(errno_t::EFAULT);

    futex_tab_ent_t *ent = futex_tab.lookup(&addr);
    futex_tab_ent_t *ent2 = futex_tab.lookup(&addr2);

    int old2 = 0;
    if (!mm_copy_user(&old2, uaddr2, sizeof(old2)))
        return -int(errno_t::EFAULT);

    for (;; __builtin_ia32_pause()) {
        // Read original value

        // Compute replacement for the *uaddr2 = *uaddr2 op oparg atomic update
        int replacement = futex_apply_op(old2, op, oparg);

        // Perform nofault compare exchange
        int xchg = mm_compare_exchange_user(uaddr2, &old2, replacement);

        // If it succeeded
        if (likely(xchg > 0))
            break;

        // If it faulted
        if (unlikely(xchg < 0))
            return -int(errno_t::EFAULT);

        // Otherwise, loop
    }

    if (ent)
        ent->wake.notify_n(wake);

    if (ent2 && futex_apply_cmp(old2, cmp, cmparg))
        ent2->wake.notify_n(wake2);

    return wake + wake2;
}

// Atomically modify a memory location (lock), presumably to
// release a mutex, wake anyone waiting for the lock,
// then wait for a wake on the condition
// location (cond). Needed for condition_variable::wait((_until)?)
static long futex_wait_op(int *lock, int op_param,
                          int *cond, int wake, uint64_t timeout_time)
{
    scoped_lock lock_(futex_lock);

    uintptr_t condaddr = mphysaddr(cond);
    uintptr_t lockaddr = mphysaddr(lock);

    op_param_t opp{op_param};
    int oparg = opp.oparg();
    int op = opp.op();

    // Read the original value of the mutex
    int lockold = 0;
    if (unlikely(!mm_copy_user(&lockold, lock, sizeof(lockold))))
        return -int(errno_t::EFAULT);

    futex_tab_ent_t *lockent = futex_tab.lookup(&lockaddr);
    futex_tab_ent_t *condent = futex_tab.lookup(&condaddr);

    for (;; __builtin_ia32_pause()) {
        // Read original value

        // Compute replacement for the *uaddr2 = *uaddr2 op oparg atomic update
        int replacement = futex_apply_op(lockold, op, oparg);

        // Perform nofault compare exchange
        int xchg = mm_compare_exchange_user(lock, &lockold, replacement);

        // If it succeeded
        if (likely(xchg > 0)) {
            if (lockent) {
                if (wake != INT_MAX)
                    lockent->wake.notify_n(wake);
                else
                    lockent->wake.notify_all();
            }

            if (condent) {
                std::chrono::steady_clock::time_point timeout_timepoint{
                    std::chrono::nanoseconds(timeout_time)};
                if (unlikely(condent->wake.wait_until(
                                 lock_, timeout_timepoint) ==
                             std::cv_status::timeout))
                    return -int(errno_t::ETIMEDOUT);
            }

            return wake;
        }

        // If it faulted
        if (unlikely(xchg < 0))
            return -int(errno_t::EFAULT);

        // There was a racing modification of *uaddr2
    }
}

static std::pair<uint64_t, bool>
timeout_from_user_timespec(timespec const *t)
{
    timespec ts{};
    if (unlikely(!mm_copy_user(&ts, t, sizeof(ts))))
        return { 0, false };

    return { t ? t->tv_sec * UINT64_C(1000000000) +
                 t->tv_nsec : UINT64_MAX, true };
}

long sys_futex(int *uaddr, int futex_op, int val,
               struct timespec const *timeout, int *uaddr2, int val3)
{
    std::pair<uint64_t, bool> timeout_ns;

    timeout_ns = { UINT64_MAX, true };

    if (timeout) {
        timeout_ns = timeout_from_user_timespec(timeout);

        if (unlikely(!timeout_ns.second))
            return -int(errno_t::EFAULT);
    }

    switch (futex_op) {
    case FUTEX_WAIT:
        return futex_wait(uaddr, val, timeout_ns.first);

    case FUTEX_WAKE:
        return futex_wake(uaddr, val);

    case FUTEX_WAKE_OP:
        int val2;
        val2 = int(intptr_t(timeout));
        return futex_wake_op(uaddr2, val3, uaddr, val, val2);

    case FUTEX_WAIT_OP:
        return futex_wait_op(uaddr2, val3, uaddr, val, timeout_ns.first);

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

    std::vector<ext::string> argv_items;
    std::vector<ext::string> envp_items;
    std::vector<ext::string> *curr_items = &argv_items;
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

void sys_exit(int exitcode)
{

    process_t::exit(-1, exitcode);
}

long sys_clone(int (*fn)(void *), void *child_stack, int flags, void *arg)
{
    process_t *this_process = thread_current_process();
    return this_process->clone(fn, child_stack, flags, arg);
}
