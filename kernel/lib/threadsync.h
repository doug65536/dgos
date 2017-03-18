#pragma once
#include "cpu/spinlock.h"
#include "thread.h"

typedef struct thread_wait_link_t thread_wait_link_t;
typedef struct condition_var_t condition_var_t;
typedef struct thread_wait_t thread_wait_t;
typedef struct mutex_t mutex_t;

// Link in chain of waiters
struct thread_wait_link_t {
    thread_wait_link_t volatile * next;
    thread_wait_link_t volatile * prev;
};

// An instance of this is maintained by waiting threads
struct thread_wait_t {
    thread_wait_link_t volatile link;

    thread_t thread;
};

struct condition_var_t {
    // Linked list of threads waiting for the condition variable
    thread_wait_link_t volatile link;

    // This lock must be held while adding a
    // thread_wait_t to the wait list
    spinlock_t lock;
};

struct mutex_t {
    thread_wait_link_t volatile link;

    thread_t volatile owner;
    int spin_count;

    // This lock must be held while updating the list
    spinlock_t lock;
};

void mutex_init(mutex_t *mutex);
void mutex_destroy(mutex_t *mutex);
int mutex_held(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

void mutex_lock_noyield(mutex_t *mutex);

void condvar_init(condition_var_t *var);
void condvar_destroy(condition_var_t *var);
void condvar_wait(condition_var_t *var, mutex_t *mutex);
void condvar_wake_one(condition_var_t *var);
void condvar_wake_all(condition_var_t *var);

void condvar_wait_spinlock(condition_var_t *var,
                           spinlock_t *spinlock);

void condvar_wait_noyield(condition_var_t *var, mutex_t *mutex);
