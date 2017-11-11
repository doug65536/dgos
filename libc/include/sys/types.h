#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

// Used for file block counts
typedef int64_t blkcnt_t;

// Used for block sizes
typedef uint32_t blksize_t;

// Used for system times in clock ticks or CLOCKS_PER_SEC (see <time.h>).
typedef int64_t clock_t;

// Used for clock ID type in the clock and timer functions.
typedef int clockid_t;

// Used for device IDs.
typedef uint64_t dev_t;

// Used for file system block counts
typedef uint64_t fsblkcnt_t;

// Used for file system file counts
typedef uint64_t fsfilcnt_t;

// Used for group IDs.
typedef uint32_t gid_t;

// Used as a general identifier; can be used to contain at least a pid_t, uid_t or a gid_t.
typedef uint32_t id_t;

// Used for file serial numbers.
typedef uint64_t ino_t;

// Used for interprocess communication.
typedef uintptr_t key_t;

// Used for some file attributes.
typedef uint32_t mode_t;

// Used for link counts.
typedef uint32_t nlink_t;

// Used for file sizes.
typedef int64_t off_t;

// Used for process IDs and process group IDs.
typedef uint32_t pid_t;

// Used to identify a thread attribute object.
typedef struct pthread_addr_t pthread_attr_t;

// Used for condition variables.
typedef _t pthread_cond_t;

// Used to identify a condition attribute object.
typedef _t pthread_condattr_t;

// Used for thread-specific data keys.
typedef _t pthread_key_t;

// Used for mutexes.
typedef _t pthread_mutex_t;

// Used to identify a mutex attribute object.
typedef _t pthread_mutexattr_t;

// Used for dynamic package initialisation.
typedef _t pthread_once_t;

// Used for read-write locks.
typedef _t pthread_rwlock_t;

// Used for read-write lock attributes.
typedef _t pthread_rwlockattr_t;

// Used to identify a thread.
typedef uint32_t pthread_t;

// Used for a count of bytes or an error indication.
typedef intptr_t ssize_t;

// Used for time in microseconds
typedef int64_t suseconds_t;

// Used for time in seconds.
typedef int64_t time_t;

// Used for timer ID returned by timer_create().
typedef intptr_t timer_t;

// Used for user IDs.
typedef uint32_t uid_t;

// Used for time in microseconds.
typedef int64_t useconds_t;

/// All of the types are defined as arithmetic types of an appropriate length, with the following exceptions:
///  key_t, pthread_attr_t, pthread_cond_t, pthread_condattr_t, pthread_key_t,
///  pthread_mutex_t, pthread_mutexattr_t, pthread_once_t, pthread_rwlock_t, pthread_rwlockattr_t.
/// Additionally, blkcnt_t and off_t are  extended signed integral types, 
/// fsblkcnt_t, fsfilcnt_t and ino_t are defined as extended unsigned integral types,
/// size_t is an unsigned integral type,
/// blksize_t, pid_t and ssize_t are signed integral types.
/// The type ssize_t is capable of storing values at least in the range [-1, SSIZE_MAX].
/// The type useconds_t is an unsigned integral type capable of storing values
///  at least in the range [0, 1,000,000].
/// The type suseconds_t is a signed integral type capable of storing values
///  at least in the range [-1, 1,000,000].
/// There are no defined comparison or assignment operators for the types 
///  pthread_attr_t, pthread_cond_t, pthread_condattr_t, pthread_mutex_t,
///  pthread_mutexattr_t, pthread_rwlock_t and pthread_rwlockattr_t.

__END_DECLS
                                                                                                          
