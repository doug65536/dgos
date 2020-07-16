#pragma once

#include <sys/stat.h>
#include <sys/types.h>

// "PT_Bad!!"
#define __PTHREAD_BAD_SIG \
    0x50545f4261642121

// "PT_Attr_"
#define __PTHREAD_ATTR_SIG \
    0x50545f417474725f

struct __pthread_attr_t {
    uint64_t sig;
    size_t guard_sz;
    bool detach;
    sched_param sched;
};

// "PT_Barri"
#define __PTHREAD_BARRIER_SIG \
    0x50545f4261727269

struct __pthread_barrier_t {
    uint64_t sig;
};

// "PT_BarAt"
#define __PTHREAD_BARRIERATTR_SIG \
    0x50545f4261724174

struct __pthread_barrierattr_t {
    uint64_t sig;
};

// "PT_CondV"
#define __PTHREAD_COND_SIG \
    0x50545f436f6e6456

struct __pthread_cond_t {
    uint64_t sig;
    int waiters;
};

#define PTHREAD_COND_INITIALIZER { __PTHREAD_COND_SIG, 0 }

#define __PTHREAD_CONDATTR_SIG

struct __pthread_condattr_t {
    uint64_t sig;
};

#define __PTHREAD_KEY_SIG

struct __pthread_key_t {
    uint64_t sig;
};

// "PT_Mutex"
#define __PTHREAD_MUTEX_SIG \
    0x50545f4d75746578

struct __pthread_mutex_t {
    uint64_t sig;
    int owner;
    int recursions;
};

#define PTHREAD_MUTEX_INITIALIZER   { __PTHREAD_MUTEX_SIG, -1, -1 }

struct __pthread_mutexattr_t {
    uint64_t sig;
};

#define __PTHREAD_ONCE_SIG \
    0x50545f4f6e636520

struct __pthread_once_t {
    uint64_t sig;
};

#define __PTHREAD_RWLOCK_SIG \
    0x50545f52574c6f63

struct __pthread_rwlock_t {
    uint64_t sig;

    // -1 for write lock held, 0 for unlocked, > 0 for reader count
    int lock_count;

    // Only used when exclusively owned, otherwise -1
    // There's also a small race window between acquiring the
    // exclusive lock and setting this field, and another race
    // window between clearing this back to -1, and releasing
    // the exclusive lock, therefore `owner` is primarily for debugging
    int owner;
};

#define PTHREAD_RWLOCK_INITIALIZER  { __PTHREAD_RWLOCK_SIG, 0, -1 };

#define __PTHREAD_RWLOCKATTR_SIG \
    0x50545f52574c4174

struct __pthread_rwlockattr_t {
    uint64_t sig;
};

#define __PTHREAD_SPINLOCK_SIG \
    0x50545f5370696e4c

struct __pthread_spinlock_t {
    uint64_t sig;
    int owner;
};

#define PTHREAD_BARRIER_SERIAL_THREAD   0
#define PTHREAD_CANCEL_ASYNCHRONOUS     0
#define PTHREAD_CANCEL_ENABLE           0
#define PTHREAD_CANCEL_DEFERRED         0
#define PTHREAD_CANCEL_DISABLE          0
#define PTHREAD_CANCELED                0
#define PTHREAD_CREATE_DETACHED         0
#define PTHREAD_CREATE_JOINABLE         0

//[TPS] [Option Start]
//PTHREAD_EXPLICIT_SCHED
//PTHREAD_INHERIT_SCHED
//[Option End]

#define PTHREAD_MUTEX_DEFAULT           0
#define PTHREAD_MUTEX_ERRORCHECK        0
#define PTHREAD_MUTEX_NORMAL            0
#define PTHREAD_MUTEX_RECURSIVE         0
#define PTHREAD_MUTEX_ROBUST            0
#define PTHREAD_MUTEX_STALLED           0
#define PTHREAD_ONCE_INIT               0

//[RPI|TPI] [Option Start]
//PTHREAD_PRIO_INHERIT
//[Option End]

//[MC1] [Option Start]
//PTHREAD_PRIO_NONE
//[Option End]

//[RPP|TPP] [Option Start]
//PTHREAD_PRIO_PROTECT
//[Option End]

#define PTHREAD_PROCESS_SHARED          0
#define PTHREAD_PROCESS_PRIVATE         0

//[TPS] [Option Start]
//PTHREAD_SCOPE_PROCESS
//PTHREAD_SCOPE_SYSTEM
//[Option End]

int   pthread_atfork(void (*)(void), void (*)(void),
          void(*)(void));
int   pthread_attr_destroy(pthread_attr_t *);
int   pthread_attr_getdetachstate(pthread_attr_t const *, int *);
int   pthread_attr_getguardsize(pthread_attr_t const *restrict,
          size_t *restrict);

//[TPS][Option Start]
//int   pthread_attr_getinheritsched(pthread_attr_t const *restrict,
//          int *restrict);
//[Option End]

int   pthread_attr_getschedparam(pthread_attr_t const *restrict,
          struct sched_param *restrict);

//[TPS][Option Start]
//int   pthread_attr_getschedpolicy(pthread_attr_t const *restrict,
//          int *restrict);
//int   pthread_attr_getscope(pthread_attr_t const *restrict,
//          int *restrict);
//[Option End]

//[TSA TSS][Option Start]
//int   pthread_attr_getstack(pthread_attr_t const *restrict,
//          void **restrict, size_t *restrict);
//[Option End]

//[TSS][Option Start]
//int   pthread_attr_getstacksize(pthread_attr_t const *restrict,
//          size_t *restrict);
//[Option End]

int   pthread_attr_init(pthread_attr_t *);
int   pthread_attr_setdetachstate(pthread_attr_t *, int);
int   pthread_attr_setguardsize(pthread_attr_t *, size_t);

//[TPS][Option Start]
//int   pthread_attr_setinheritsched(pthread_attr_t *, int);
//[Option End]

int   pthread_attr_setschedparam(pthread_attr_t *restrict,
          struct sched_param const *restrict);

//[TPS][Option Start]
//int   pthread_attr_setschedpolicy(pthread_attr_t *, int);
//int   pthread_attr_setscope(pthread_attr_t *, int);
//[Option End]

//[TSA TSS][Option Start]
//int   pthread_attr_setstack(pthread_attr_t *, void *, size_t);
//[Option End]

//[TSS][Option Start]
//int   pthread_attr_setstacksize(pthread_attr_t *, size_t);
//[Option End]

int   pthread_barrier_destroy(pthread_barrier_t *);
int   pthread_barrier_init(pthread_barrier_t *restrict,
          pthread_barrierattr_t const *restrict, unsigned);
int   pthread_barrier_wait(pthread_barrier_t *);
int   pthread_barrierattr_destroy(pthread_barrierattr_t *);

//[TSH][Option Start]
//int   pthread_barrierattr_getpshared(
//          pthread_barrierattr_t const *restrict, int *restrict);
//[Option End]

int   pthread_barrierattr_init(pthread_barrierattr_t *);

//[TSH][Option Start]
//int   pthread_barrierattr_setpshared(pthread_barrierattr_t *, int);
//[Option End]

int   pthread_cancel(pthread_t);
int   pthread_cond_broadcast(pthread_cond_t *);
int   pthread_cond_destroy(pthread_cond_t *);
int   pthread_cond_init(pthread_cond_t *restrict,
          pthread_condattr_t const *restrict);
int   pthread_cond_signal(pthread_cond_t *);
int   pthread_cond_timedwait(pthread_cond_t *restrict,
          pthread_mutex_t *restrict, struct timespec const *restrict);
int   pthread_cond_wait(pthread_cond_t *restrict,
          pthread_mutex_t *restrict);
int   pthread_condattr_destroy(pthread_condattr_t *);
int   pthread_condattr_getclock(pthread_condattr_t const *restrict,
          clockid_t *restrict);

//[TSH][Option Start]
//int   pthread_condattr_getpshared(pthread_condattr_t const *restrict,
//          int *restrict);
//[Option End]

int   pthread_condattr_init(pthread_condattr_t *);
int   pthread_condattr_setclock(pthread_condattr_t *, clockid_t);

//[TSH][Option Start]
//int   pthread_condattr_setpshared(pthread_condattr_t *, int);
//[Option End]

int   pthread_create(pthread_t *restrict, pthread_attr_t const *restrict,
          void *(*)(void*), void *restrict);
int   pthread_detach(pthread_t);
int   pthread_equal(pthread_t, pthread_t);

__attribute__((__noreturn__))
void  pthread_exit(void *);

//[OB XSI][Option Start]
//int   pthread_getconcurrency(void);
//[Option End]

//[TCT][Option Start]
//int   pthread_getcpuclockid(pthread_t, clockid_t *);
//[Option End]

//[TPS][Option Start]
//int   pthread_getschedparam(pthread_t, int *restrict,
//          struct sched_param *restrict);
//[Option End]

void *pthread_getspecific(pthread_key_t);
int   pthread_join(pthread_t, void **);
int   pthread_key_create(pthread_key_t *, void (*)(void*));
int   pthread_key_delete(pthread_key_t);
int   pthread_mutex_consistent(pthread_mutex_t *);
int   pthread_mutex_destroy(pthread_mutex_t *);

//[RPP|TPP][Option Start]
//int   pthread_mutex_getprioceiling(pthread_mutex_t const *restrict,
//          int *restrict);
//[Option End]

int   pthread_mutex_init(pthread_mutex_t *restrict,
          pthread_mutexattr_t const *restrict);
int   pthread_mutex_lock(pthread_mutex_t *);

//[RPP|TPP][Option Start]
//int   pthread_mutex_setprioceiling(pthread_mutex_t *restrict, int,
//          int *restrict);
//[Option End]

int   pthread_mutex_timedlock(pthread_mutex_t *restrict,
          struct timespec const *restrict);
int   pthread_mutex_trylock(pthread_mutex_t *);
int   pthread_mutex_unlock(pthread_mutex_t *);
int   pthread_mutexattr_destroy(pthread_mutexattr_t *);

//[RPP|TPP][Option Start]
//int   pthread_mutexattr_getprioceiling(
//          pthread_mutexattr_t const *restrict, int *restrict);
//[Option End]

//[MC1][Option Start]
//int   pthread_mutexattr_getprotocol(pthread_mutexattr_t const *restrict,
//          int *restrict);
//[Option End]

//[TSH][Option Start]
//int   pthread_mutexattr_getpshared(pthread_mutexattr_t const *restrict,
//          int *restrict);
//[Option End]

int   pthread_mutexattr_getrobust(pthread_mutexattr_t const *restrict,
          int *restrict);
int   pthread_mutexattr_gettype(pthread_mutexattr_t const *restrict,
          int *restrict);
int   pthread_mutexattr_init(pthread_mutexattr_t *);

//[RPP|TPP][Option Start]
//int   pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
//[Option End]

//[MC1][Option Start]
//int   pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
//[Option End]

//[TSH][Option Start]
//int   pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
//[Option End]

int   pthread_mutexattr_setrobust(pthread_mutexattr_t *, int);
int   pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int   pthread_once(pthread_once_t *, void (*)(void));
int   pthread_rwlock_destroy(pthread_rwlock_t *);
int   pthread_rwlock_init(pthread_rwlock_t *restrict,
          pthread_rwlockattr_t const *restrict);
int   pthread_rwlock_rdlock(pthread_rwlock_t *);
int   pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict,
          struct timespec const *restrict);
int   pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict,
          struct timespec const *restrict);
int   pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int   pthread_rwlock_trywrlock(pthread_rwlock_t *);
int   pthread_rwlock_unlock(pthread_rwlock_t *);
int   pthread_rwlock_wrlock(pthread_rwlock_t *);
int   pthread_rwlockattr_destroy(pthread_rwlockattr_t *);

//[TSH][Option Start]
//int   pthread_rwlockattr_getpshared(
//          pthread_rwlockattr_t const *restrict, int *restrict);
//[Option End]

int   pthread_rwlockattr_init(pthread_rwlockattr_t *);

//[TSH][Option Start]
//int   pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
//[Option End]

pthread_t
      pthread_self(void);
int   pthread_setcancelstate(int, int *);
int   pthread_setcanceltype(int, int *);

//[OB XSI][Option Start]
//int   pthread_setconcurrency(int);
//[Option End]

//[TPS][Option Start]
//int   pthread_setschedparam(pthread_t, int,
//          struct sched_param const *);
//int   pthread_setschedprio(pthread_t, int);
//[Option End]

int   pthread_setspecific(pthread_key_t, void const *);
int   pthread_spin_destroy(pthread_spinlock_t *);
int   pthread_spin_init(pthread_spinlock_t *, int);
int   pthread_spin_lock(pthread_spinlock_t *);
int   pthread_spin_trylock(pthread_spinlock_t *);
int   pthread_spin_unlock(pthread_spinlock_t *);
void  pthread_testcancel(void);

// ---

int __clone(void (*bootstrap)(int tid, void *(fn)(void*), void *arg),
            void *child_stack,
            int flags, void *fn, void *arg);

int __futex(int *uaddr, int futex_op, int val,
            struct timespec const *timeout, int *uaddr2, int val3);

#define __FUTEX_PRIVATE_FLAG    0x80000000
#define __FUTEX_WAIT            0x00000001
#define __FUTEX_WAKE            0x00000002
#define __FUTEX_WAKE_OP         0x00000003
#define __FUTEX_WAIT_OP         0x00000004

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

int __clone(void (*bootstrap)(int tid, void *(*fn)(void*), void *arg),
            void *child_stack, int flags,
            void *(*fn)(void *arg), void *arg);

void __pthread_set_tid(pthread_t tid);
