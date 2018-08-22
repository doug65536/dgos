#pragma once
#include "types.h"
#include "assert.h"

enum struct errno_t : int8_t {
    OK = 0,

    /// Argument list too long (POSIX.1).
    E2BIG = 1,

    /// Permission denied (POSIX.1).
    EACCES = 2,

    /// Address already in use (POSIX.1).
    EADDRINUSE = 3,

    /// Address not available (POSIX.1).
    EADDRNOTAVAIL = 4,

    /// Address family not supported (POSIX.1).
    EAFNOSUPPORT = 5,

    /// Resource temporarily unavailable
    /// (may be the same value as EWOULDBLOCK) (POSIX.1).
    EAGAIN = 6,

    /// Connection already in progress (POSIX.1).
    EALREADY = 7,

    /// Invalid exchange.
    EBADE = 8,

    /// Bad file descriptor (POSIX.1).
    EBADF = 9,

    /// File descriptor in bad state.
    EBADFD = 10,

    /// Bad message (POSIX.1).
    EBADMSG = 11,

    /// Invalid request descriptor.
    EBADR = 12,

    /// Invalid request code.
    EBADRQC = 13,

    /// Invalid slot.
    EBADSLT = 14,

    /// Device or resource busy (POSIX.1).
    EBUSY = 15,

    /// Operation canceled (POSIX.1).
    ECANCELED = 16,

    /// No child processes (POSIX.1).
    ECHILD = 17,

    /// Channel number out of range.
    ECHRNG = 18,

    /// Communication error on send.
    ECOMM = 19,

    /// Connection aborted (POSIX.1).
    ECONNABORTED = 20,

    /// Connection refused (POSIX.1).
    ECONNREFUSED = 21,

    /// Connection reset (POSIX.1).
    ECONNRESET = 22,

    /// Resource deadlock avoided (POSIX.1).
    EDEADLK = 23,

    /// Synonym for EDEADLK.
    EDEADLOCK = errno_t::EDEADLK,

    /// Destination address required (POSIX.1).
    EDESTADDRREQ = 24,

    /// Mathematics argument out of domain of function (POSIX.1, C99).
    EDOM = 25,

    /// Disk quota exceeded (POSIX.1).
    EDQUOT = 26,

    /// File exists (POSIX.1).
    EEXIST = 27,

    /// Bad address (POSIX.1).
    EFAULT = 28,

    /// File too large (POSIX.1).
    EFBIG = 29,

    /// Host is down.
    EHOSTDOWN = 30,

    /// Host is unreachable (POSIX.1).
    EHOSTUNREACH = 31,

    /// Identifier removed (POSIX.1).
    EIDRM = 32,

    /// Invalid or incomplete multibyte or wide character (POSIX.1, C99).
    EILSEQ = 33,

    /// Operation in progress (POSIX.1).
    EINPROGRESS = 34,

    /// Interrupted function call (POSIX.1); see signal(7).
    EINTR = 35,

    /// Invalid argument (POSIX.1).
    EINVAL = 36,

    /// Input/output error (POSIX.1).
    EIO = 37,

    /// Socket is connected (POSIX.1).
    EISCONN = 38,

    /// Is a directory (POSIX.1).
    EISDIR = 39,

    /// Is a named type file.
    EISNAM = 40,

    /// Key has expired.
    EKEYEXPIRED = 41,

    /// Key was rejected by service.
    EKEYREJECTED = 42,

    /// Key has been revoked.
    EKEYREVOKED = 43,

    /// Level 2 halted.
    EL2HLT = 44,

    /// Level 2 not synchronized.
    EL2NSYNC = 45,

    /// Level 3 halted.
    EL3HLT = 46,

    /// Level 3 halted.
    EL3RST = 47,

    /// Cannot access a needed shared library.
    ELIBACC = 48,

    /// Accessing a corrupted shared library.
    ELIBBAD = 49,

    /// Attempting to link in too many shared libraries.
    ELIBMAX = 50,

    /// .lib section in a.out corrupted
    ELIBSCN = 51,

    /// Cannot exec a shared library directly.
    ELIBEXEC = 52,

    /// Too many levels of symbolic links (POSIX.1).
    ELOOP = 53,

    /// Wrong medium type.
    EMEDIUMTYPE = 54,

    /// Too many open files (POSIX.1).
    /// Commonly caused by exceeding the RLIMIT_NOFILE resource limit
    /// described in getrlimit(2).
    EMFILE = 55,

    /// Too many links (POSIX.1).
    EMLINK = 56,

    /// Message too long (POSIX.1).
    EMSGSIZE = 57,

    /// Multihop attempted (POSIX.1).
    EMULTIHOP = 58,

    /// Filename too long (POSIX.1).
    ENAMETOOLONG = 59,

    /// Network is down (POSIX.1).
    ENETDOWN = 60,

    /// Connection aborted by network (POSIX.1).
    ENETRESET = 61,

    /// Network unreachable (POSIX.1).
    ENETUNREACH = 62,

    /// Too many open files in system (POSIX.1).
    /// On Linux, this is probably a result of encountering the
    ///  /proc/sys/fs/file-max limit (see proc(5)).
    ENFILE = 63,

    /// No buffer space available (POSIX.1 (XSI STREAMS option)).
    ENOBUFS = 64,

    /// No message is available on the STREAM head read queue (POSIX.1).
    ENODATA = 65,

    /// No such device (POSIX.1).
    ENODEV = 66,

    /// No such file or directory (POSIX.1).
    /// Typically, this error results when a specified,
    /// pathname does not exist, or one of the components in
    /// the directory prefix of a pathname does not exist, or
    /// the specified pathname is a dangling symbolic link.
    ENOENT = 67,

    /// Exec format error (POSIX.1).
    ENOEXEC = 68,

    /// Required key not available.
    ENOKEY = 69,

    /// No locks available (POSIX.1).
    ENOLCK = 70,

    /// Link has been severed (POSIX.1).
    ENOLINK = 71,

    /// No medium found.
    ENOMEDIUM = 72,

    /// Not enough space (POSIX.1).
    ENOMEM = 73,

    /// No message of the desired type (POSIX.1).
    ENOMSG = 74,

    /// Machine is not on the network.
    ENONET = 75,

    /// Package not installed.
    ENOPKG = 76,

    /// Protocol not available (POSIX.1).
    ENOPROTOOPT = 77,

    /// No space left on device (POSIX.1).
    ENOSPC = 78,

    /// No STREAM resources (POSIX.1 (XSI STREAMS option)).
    ENOSR = 79,

    /// Not a STREAM (POSIX.1 (XSI STREAMS option)).
    ENOSTR = 80,

    /// Function not implemented (POSIX.1).
    ENOSYS = 81,

    /// Block device required.
    ENOTBLK = 82,

    /// The socket is not connected (POSIX.1).
    ENOTCONN = 83,

    /// Not a directory (POSIX.1).
    ENOTDIR = 84,

    /// Directory not empty (POSIX.1).
    ENOTEMPTY = 85,

    /// Not a socket (POSIX.1).
    ENOTSOCK = 86,

    /// Operation not supported (POSIX.1).
    ENOTSUP = 87,

    /// Inappropriate I/O control operation (POSIX.1).
    ENOTTY = 88,

    /// Name not unique on network.
    ENOTUNIQ = 89,

    /// No such device or address (POSIX.1).
    ENXIO = 90,

    /// Operation not supported on socket (POSIX.1).
    /// (ENOTSUP and EOPNOTSUPP have the same value on Linux,
    /// but according to POSIX.1 these error values should be
    /// distinct.)
    EOPNOTSUPP = 91,

    /// Value too large to be stored in data type (POSIX.1).
    EOVERFLOW = 92,

    /// Operation not permitted (POSIX.1).
    EPERM = 93,

    /// Protocol family not supported.
    EPFNOSUPPORT = 94,

    /// Broken pipe (POSIX.1).
    EPIPE = 95,

    /// Protocol error (POSIX.1).
    EPROTO = 96,

    /// Protocol not supported (POSIX.1).
    EPROTONOSUPPORT = 97,

    /// Protocol wrong type for socket (POSIX.1).
    EPROTOTYPE = 98,

    /// Result too large (POSIX.1, C99).
    ERANGE = 99,

    /// Remote address changed.
    EREMCHG = 100,

    /// Object is remote.
    EREMOTE = 101,

    /// Remote I/O error.
    EREMOTEIO = 102,

    /// Interrupted system call should be restarted.
    ERESTART = 103,

    /// Read-only filesystem (POSIX.1).
    EROFS = 104,

    /// Cannot send after transport endpoint shutdown.
    ESHUTDOWN = 105,

    /// Invalid seek (POSIX.1).
    ESPIPE = 106,

    /// Socket type not supported.
    ESOCKTNOSUPPORT = 107,

    /// No such process (POSIX.1).
    ESRCH = 108,

    /// Stale file handle (POSIX.1).
    /// This error can occur for NFS and for other
    /// filesystems.
    ESTALE = 109,

    /// Streams pipe error.
    ESTRPIPE = 110,

    /// Timer expired.  (POSIX.1 (XSI STREAMS option))
    /// (POSIX.1 says "STREAM ioctl(2) timeout")
    ETIME = 111,

    /// Connection timed out (POSIX.1).
    ETIMEDOUT = 112,

    /// Text file busy (POSIX.1).
    ETXTBSY = 113,

    /// Structure needs cleaning.
    EUCLEAN = 114,

    /// Protocol driver not attached.
    EUNATCH = 115,

    /// Too many users.
    EUSERS = 116,

    /// Operation would block (may be same value as EAGAIN) (POSIX.1).
    EWOULDBLOCK = 117,

    /// Improper link (POSIX.1).
    EXDEV = 118,

    /// Exchange full.
    EXFULL = 119,

    // Extensions

    // Unable to accept/provide data at the required rate
    EOVERLOAD = 120,

    // Insufficient bandwidth
    EBANDWIDTH = 121,

    // Stopped
    ESTOPPED = 122,

    // Did not receive expected amount of data
    ESHORT = 123,

    MAX_ERRNO = 124
};
