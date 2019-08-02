#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

__BEGIN_DECLS

// The <fcntl.h> header shall define the following symbolic constants for the
// cmd argument used by fcntl(). The values shall be unique and shall be
// suitable for use in #if preprocessing directives.

// Duplicate file descriptor.
#define F_DUPFD     1

// Duplicate file descriptor with the close-on- exec flag FD_CLOEXEC set.
#define F_DUPFD_CLOEXEC 2

// Get file descriptor flags.
#define F_GETFD 3

// Set file descriptor flags.
#define F_SETFD 4

// Get file status flags and file access modes.
#define F_GETFL 5

// Set file status flags.
#define F_SETFL 6

// Get record locking information.
#define F_GETLK 7

// Set record locking information.
#define F_SETLK 8

// Set record locking information; wait if blocked.
#define F_SETLKW 9

// Get process or process group ID to receive SIGURG signals.
#define F_GETOWN 10

// Set process or process group ID to receive SIGURG signals.
#define F_SETOWN 11

// The <fcntl.h> header shall define the following symbolic constant used for
// the fcntl() file descriptor flags, which shall be suitable for use in
// #if preprocessing directives.

// Close the file descriptor upon execution of an exec family function.
#define FD_CLOEXEC 1

// The <fcntl.h> header shall also define the following symbolic constants
// for the l_type argument used for record locking with fcntl().
// The values shall be unique and shall be suitable for use in
// #if preprocessing directives.

// Unlock.
#define F_UNLCK 1

// Shared or read lock.
#define F_RDLCK 2

// Exclusive or write lock.
#define F_WRLCK 3

// The <fcntl.h> header shall define the values used for l_whence,
// SEEK_SET, SEEK_CUR, and SEEK_END as described in <stdio.h>.

// The <fcntl.h> header shall define the following symbolic constants as
// file creation flags for use in the oflag value to open() and openat().
// The values shall be bitwise-distinct and shall be suitable for use in
// #if preprocessing directives.

#define O_RDONLY    (1<<0)
#define O_WRONLY    (1<<1)
#define O_RDWR      (O_RDONLY|O_WRONLY)
#define O_APPEND    (1<<2)
#define O_ASYNC     (1<<3)
#define O_CLOEXEC   (1<<4)
#define O_CREAT     (1<<5)
#define O_DIRECT    (1<<6)
#define O_DIRECTORY (1<<7)
#define O_DSYNC     (1<<8)
#define O_EXCL      (1<<9)
#define O_LARGEFILE (1<<10)
#define O_NOATIME   (1<<11)
#define O_NOCTTY    (1<<12)
#define O_NOFOLLOW  (1<<13)
#define O_NBLOCK    (1<<14)
#define O_NDELAY    O_NBLOCK
#define O_PATH      (1<<15)
#define O_SYNC      (1<<16)
#define O_TMPFILE   (1<<17)
#define O_TRUNC     (1<<18)

// The FD_CLOEXEC flag associated with the new descriptor shall be set to
// close the file descriptor upon execution of an exec family function.
#define O_CLOEXEC (1<<4)

// Create file if it does not exist.
#define O_CREAT (1<<5)

// Fail if file is a non-directory file.
#define O_DIRECTORY (1<<7)

// Exclusive use flag.
#define O_EXCL (1<<9)

// Do not assign controlling terminal.
#define O_NOCTTY (1<<12)

// Do not follow symbolic links.
#define O_NOFOLLOW (1<<13)

// Truncate flag.
#define O_TRUNC (1<<18)

// Set the termios structure terminal parameters to a state that provides
// conforming behavior; see Parameters that Can be Set.
// The O_TTY_INIT flag can have the value zero and in this case it need not
// be bitwise-distinct from the other flags.
#define O_TTY_INIT ?

// The <fcntl.h> header shall define the following symbolic constants for use
// as file status flags for open(), openat(), and fcntl(). The values shall be
// suitable for use in #if preprocessing directives.

// Set append mode.
#define O_APPEND (1<<2)

// Write according to synchronized I/O data integrity completion.
#define O_DSYNC (1<<8)

// Non-blocking mode.
#define O_NONBLOCK (1<<14)

// Synchronized read I/O operations.
#define O_RSYNC (1<<8)

// Write according to synchronized I/O file integrity completion.
#define O_SYNC (1<<16)

// The <fcntl.h> header shall define the following symbolic constant for use
// as the mask for file access modes. The value shall be suitable for use in
// #if preprocessing directives.

// Mask for file access modes.
#define O_ACCMODE

// The <fcntl.h> header shall define the following symbolic constants for use
// as the file access modes for open(), openat(), and fcntl(). The values
// shall be unique, except that O_EXEC and O_SEARCH may have equal values.
// The values shall be suitable for use in #if preprocessing directives.

// Open for execute only (non-directory files). The result is unspecified if
// this flag is applied to a directory.
#define O_EXEC

// Open for reading only.
//#define O_RDONLY

// Open for reading and writing.
#define O_RDWR

// Open directory for search only. The result is unspecified if this flag is
// applied to a non-directory file.
#define O_SEARCH

// Open for writing only.
#define O_WRONLY

// The <fcntl.h> header shall define the symbolic constants for file modes
// for use as values of mode_t as described in <sys/stat.h>.

// The <fcntl.h> header shall define the following symbolic constant as a
// special value used in place of a file descriptor for the *at() functions
// which take a directory file descriptor as a parameter:

// Use the current working directory to determine the target of relative
// file paths.
#define AT_FDCWD

// The <fcntl.h> header shall define the following symbolic constant as a
// value for the flag used by faccessat():

// Check access using effective user and group ID.
#define AT_EACCESS

// The <fcntl.h> header shall define the following symbolic constant as a
// value for the flag used by fstatat(), fchmodat(), fchownat(),
// and utimensat():

// Do not follow symbolic links.
#define AT_SYMLINK_NOFOLLOW

// The <fcntl.h> header shall define the following symbolic constant as a
// value for the flag used by linkat():

// Follow symbolic link.
#define AT_SYMLINK_FOLLOW

// The <fcntl.h> header shall define the following symbolic constant as a
// value for the flag used by unlinkat():

// Remove directory instead of file.
#define AT_REMOVEDIR

// The <fcntl.h> header shall define the following symbolic constants for the
// advice argument used by posix_fadvise():

// The application expects that it will not access the specified data in the
// near future.
#define POSIX_FADV_DONTNEED (-2)

// The application expects to access the specified data once and then not
// reuse it thereafter.
#define POSIX_FADV_NOREUSE (0)

// The application has no advice to give on its behavior with respect to the
// specified data. It is the default characteristic if no advice is
// given for an open file.
#define POSIX_FADV_NORMAL (0)

// The application expects to access the specified data in a random order.
#define POSIX_FADV_RANDOM (-1)

// The application expects to access the specified data sequentially from
// lower offsets to higher offsets.
#define POSIX_FADV_SEQUENTIAL (1)

// The application expects to access the specified data in the near future.
#define POSIX_FADV_WILLNEED (2)

// The <fcntl.h> header shall define the flock structure describing a
// file lock. It shall include the following members:

struct flock {
    // Type of lock; F_RDLCK, F_WRLCK, F_UNLCK.
    short  l_type;

    // Flag for starting offset.
    short  l_whence;

    // Relative offset in bytes.
    off_t  l_start;

    // Size; if 0 then until EOF.
    off_t  l_len;

    // Process ID of the process holding the lock; returned with F_GETLK.
    pid_t  l_pid;
};

// The <fcntl.h> header shall define the mode_t, off_t, and pid_t types
// as described in <sys/types.h>.

// The following shall be declared as functions and may also be defined
// as macros. Function prototypes shall be provided.

int  creat(char const *path, mode_t mode);
int  fcntl(int fd, int cmd, ...);
int  open(char const *path, int flags, ...);
int  openat(int dirfd, char const *path, int flags, ...);

int  posix_fadvise(int, off_t offset, off_t len, int advice);
int  posix_fallocate(int, off_t, off_t);

__END_DECLS
