#pragma once
#include "cpu/spinlock.h"
#include "thread.h"

typedef struct condition_var_link_t condition_var_link_t;
struct condition_var_link_t {
    condition_var_link_t volatile * next;
    condition_var_link_t volatile * prev;
};

typedef struct condition_var_wait_t {
    condition_var_link_t volatile link;
    thread_t thread;
} condition_var_wait_t;

typedef struct condition_var_t {
    // Circular linked list root
    condition_var_link_t volatile link;
} condition_var_t;

typedef struct mutex_t {
    thread_t owner;

    // Lock is held only to add to the waiters list
    spinlock_t lock;
    condition_var_t waiters;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

void condvar_init(condition_var_t *var);
void condvar_destroy(condition_var_t *var);
void condvar_wait(condition_var_t *var, mutex_t *mutex);
void condvar_wake_one(condition_var_t *var);
void condvar_wake_all(condition_var_t *var);
