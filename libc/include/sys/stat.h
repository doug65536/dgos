#pragma once

#include <sys/types.h>
#include <sys/time.h>

/// The <sys/stat.h> header shall define the structure of the data returned by the fstat(), lstat(), and stat() functions.
///
/// The <sys/stat.h> header shall define the stat structure, which shall include at least the following members:
///

struct stat {
    // Device ID of device containing file.
    dev_t st_dev;

    // File serial number.
    ino_t st_ino;

    // Mode of file (see below).
    mode_t st_mode;

    // Number of hard links to the file.
    nlink_t st_nlink;

    // User ID of file.
    uid_t st_uid;

    // Group ID of file.
    gid_t st_gid;

    // Device ID (if file is character or block special).
    dev_t st_rdev;

    // For regular files, the file size in bytes.
    // For symbolic links, the length in bytes of the
    // pathname contained in the symbolic link.
    // For a shared memory object, the length in bytes.
    // For a typed memory object, the length in bytes.
    // For other file types, the use of this field is
    // unspecified.
    off_t st_size;

    struct timespec st_atim;// Last data access timestamp.
    struct timespec st_mtim;// Last data modification timestamp.
    struct timespec st_ctim;// Last file status change timestamp.

    blksize_t st_blksize;   // A file system-specific preferred I/O block size
                            // for this object. In some file system types, this
                            // may vary from file to file.

    blkcnt_t st_blocks;     // Number of blocks allocated for this object.
};

///
/// The st_ino and st_dev fields taken together uniquely identify the file within the system.
///
/// The <sys/stat.h> header shall define the timespec structure as described in <time.h>. Times shall be given in seconds since the Epoch.
///
/// Which structure members have meaningful values depends on the type of file.
///  For further information, see the descriptions of fstat(), lstat(),
///  and stat() in the System Interfaces volume of POSIX.1-2017.
///
/// For compatibility with earlier versions of this standard, the st_atime
/// macro shall be defined with the value st_atim.tv_sec. Similarly, st_ctime and st_mtime shall be defined as macros with the values st_ctim.tv_sec and st_mtim.tv_sec, respectively.
///
/// The <sys/stat.h> header shall define the following symbolic constants
/// for the file types encoded in type mode_t. The values shall be suitable
/// for use in #if preprocessing directives:
///
/// S_IFMT
///     [XSI] [Option Start] Type of file.
///
// Block special
#define S_IFBLK 010000
// Character special.
#define S_IFCHR 020000
/// FIFO special.
#define S_IFIFO 040000
/// Regular.
#define S_IFREG 0100000
/// Directory.
#define S_IFDIR 0200000
/// Symbolic link.
#define S_IFLNK 0400000
/// Socket.
#define S_IFSOCK 1000000
///
/// The <sys/stat.h> header shall define the following symbolic constants for the file mode bits encoded in type mode_t, with the indicated numeric values. These macros shall expand to an expression which has a type that allows them to be used, either singly or OR'ed together, as the third argument to open() without the need for a mode_t cast. The values shall be suitable for use in #if preprocessing directives.
///
/// Name
///
///
/// Numeric Value
///
///
/// Description
///
#define S_IRWXU 0700
///
///
/// Read, write, execute/search by owner.
///
#define S_IRUSR 0400
///
///
/// Read permission, owner.
///
#define S_IWUSR 0200
///
///
/// Write permission, owner.
///
#define S_IXUSR 0100
///
///
/// Execute/search permission, owner.
///
#define S_IRWXG 070
///
///
/// Read, write, execute/search by group.
///
#define S_IRGRP 040
///
///
/// Read permission, group.
///
#define S_IWGRP 020
///
///
/// Write permission, group.
///
#define S_IXGRP 010
///
///
/// Execute/search permission, group.
///
#define S_IRWXO 07
///
///
/// Read, write, execute/search by others.
///
#define S_IROTH 04
///
///
/// Read permission, others.
///
#define S_IWOTH 02
///
///
/// Write permission, others.
///
#define S_IXOTH 01
///
///
/// Execute/search permission, others.
///
#define S_ISUID 04000
///
///
/// Set-user-ID on execution.
///
#define S_ISGID 02000
///
///
/// Set-group-ID on execution.
/// On directories, restricted deletion flag.
///
#define S_ISVTX 01000
///
/// The following macros shall be provided to test whether a file is of the specified type. The value m supplied to the macros is the value of st_mode from a stat structure. The macro shall evaluate to a non-zero value if the test is true; 0 if the test is false.
///

// Test for a block special file.
#define S_ISBLK(m) (((m) & S_IFBLK) != 0)
// Test for a character special file.
#define S_ISCHR(m) (((m) & S_IFBLK) != 0)
// Test for a directory.
#define S_ISDIR(m) (((m) & S_IFDIR) != 0)
// Test for a pipe or FIFO special file.
#define S_ISFIFO(m) (((m) & S_IFIFO) != 0)
// Test for a regular file.
#define S_ISREG(m) (((m) & S_IFREG) != 0)
// Test for a symbolic link.
#define S_ISLNK(m) (((m) & S_IFLNK) != 0)
// Test for a socket.
#define S_ISSOCK(m) (((m) & S_IFSOCK) != 0)

///
/// The implementation may implement message queues, semaphores, or shared memory objects as distinct file types. The following macros shall be provided to test whether a file is of the specified type. The value of the buf argument supplied to the macros is a pointer to a stat structure. The macro shall evaluate to a non-zero value if the specified object is implemented as a distinct file type and the specified file type is contained in the stat structure referenced by buf. Otherwise, the macro shall evaluate to zero.
///
/// S_TYPEISMQ(buf)
///     Test for a message queue.
/// S_TYPEISSEM(buf)
///     Test for a semaphore.
/// S_TYPEISSHM(buf)
///     Test for a shared memory object.
///
/// [TYM] [Option Start] The implementation may implement typed memory objects as distinct file types, and the following macro shall test whether a file is of the specified type. The value of the buf argument supplied to the macros is a pointer to a stat structure. The macro shall evaluate to a non-zero value if the specified object is implemented as a distinct file type and the specified file type is contained in the stat structure referenced by buf. Otherwise, the macro shall evaluate to zero.
///
/// S_TYPEISTMO(buf)
///     Test macro for a typed memory object.
///
/// [Option End]
///
/// The <sys/stat.h> header shall define the following symbolic constants as distinct integer values outside of the range [0,999999999], for use with the futimens() and utimensat() functions: UTIME_NOW UTIME_OMIT
///
/// The following shall be declared as functions and may also be defined as macros. Function prototypes shall be provided.
///
int    chmod(char const *, mode_t);
int    fchmod(int, mode_t);
int    fchmodat(int, char const *, mode_t, int);
int    fstat(int, struct stat *);
int    fstatat(int, char const *restrict, struct stat *restrict, int);
int    futimens(int, const struct timespec [2]);
int    lstat(char const *restrict, struct stat *restrict);
int    mkdir(char const *, mode_t);
int    mkdirat(int, char const *, mode_t);
int    mkfifo(char const *, mode_t);
int    mkfifoat(int, char const *, mode_t);
int    mknod(char const *, mode_t, dev_t);
int    mknodat(int, char const *, mode_t, dev_t);
int    stat(char const *restrict, struct stat *restrict);
mode_t umask(mode_t);
int    utimensat(int, char const *, const struct timespec [2], int);

///
/// Inclusion of the <sys/stat.h> header may make visible all symbols from the <time.h> header.
///
