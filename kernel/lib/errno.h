#include "types.h"
#include "assert.h"

enum struct errno_t : int8_t {
    /// Argument list too long (POSIX.1).
    E2BIG = 1,

    /// Permission denied (POSIX.1).
    EACCES,

    /// Address already in use (POSIX.1).
    EADDRINUSE,

    /// Address not available (POSIX.1).
    EADDRNOTAVAIL,

    /// Address family not supported (POSIX.1).
    EAFNOSUPPORT,

    /// Resource temporarily unavailable (may be the same value as EWOULDBLOCK) (POSIX.1).
    EAGAIN,

    /// Connection already in progress (POSIX.1).
    EALREADY,

    /// Invalid exchange.
    EBADE,

    /// Bad file descriptor (POSIX.1).
    EBADF,

    /// File descriptor in bad state.
    EBADFD,

    /// Bad message (POSIX.1).
    EBADMSG,

    /// Invalid request descriptor.
    EBADR,

    /// Invalid request code.
    EBADRQC,

    /// Invalid slot.
    EBADSLT,

    /// Device or resource busy (POSIX.1).
    EBUSY,

    /// Operation canceled (POSIX.1).
    ECANCELED,

    /// No child processes (POSIX.1).
    ECHILD,

    /// Channel number out of range.
    ECHRNG,

    /// Communication error on send.
    ECOMM,

    /// Connection aborted (POSIX.1).
    ECONNABORTED,

    /// Connection refused (POSIX.1).
    ECONNREFUSED,

    /// Connection reset (POSIX.1).
    ECONNRESET,

    /// Resource deadlock avoided (POSIX.1).
    EDEADLK,

    /// Synonym for EDEADLK.
    EDEADLOCK = errno_t::EDEADLK,

    /// Destination address required (POSIX.1).
    EDESTADDRREQ,

    /// Mathematics argument out of domain of function (POSIX.1, C99).
    EDOM,

    /// Disk quota exceeded (POSIX.1).
    EDQUOT,

    /// File exists (POSIX.1).
    EEXIST,

    /// Bad address (POSIX.1).
    EFAULT,

    /// File too large (POSIX.1).
    EFBIG,

    /// Host is down.
    EHOSTDOWN,

    /// Host is unreachable (POSIX.1).
    EHOSTUNREACH,

    /// Identifier removed (POSIX.1).
    EIDRM,

    /// Invalid or incomplete multibyte or wide character (POSIX.1, C99).
    EILSEQ,

    /// Operation in progress (POSIX.1).
    EINPROGRESS,

    /// Interrupted function call (POSIX.1); see signal(7).
    EINTR,

    /// Invalid argument (POSIX.1).
    EINVAL,

    /// Input/output error (POSIX.1).
    EIO,

    /// Socket is connected (POSIX.1).
    EISCONN,

    /// Is a directory (POSIX.1).
    EISDIR,

    /// Is a named type file.
    EISNAM,

    /// Key has expired.
    EKEYEXPIRED,

    /// Key was rejected by service.
    EKEYREJECTED,

    /// Key has been revoked.
    EKEYREVOKED,

    /// Level 2 halted.
    EL2HLT,

    /// Level 2 not synchronized.
    EL2NSYNC,

    /// Level 3 halted.
    EL3HLT,

    /// Level 3 halted.
    EL3RST,

    /// Cannot access a needed shared library.
    ELIBACC,

    /// Accessing a corrupted shared library.
    ELIBBAD,

    /// Attempting to link in too many shared libraries.
    ELIBMAX,

    /// .lib section in a.out corrupted
    ELIBSCN,

    /// Cannot exec a shared library directly.
    ELIBEXEC,

    /// Too many levels of symbolic links (POSIX.1).
    ELOOP,

    /// Wrong medium type.
    EMEDIUMTYPE,

    /// Too many open files (POSIX.1).  Commonly caused by exceeding the RLIMIT_NOFILE resource limit described in getrlimit(2).
    EMFILE,

    /// Too many links (POSIX.1).
    EMLINK,

    /// Message too long (POSIX.1).
    EMSGSIZE,

    /// Multihop attempted (POSIX.1).
    EMULTIHOP,

    /// Filename too long (POSIX.1).
    ENAMETOOLONG,

    /// Network is down (POSIX.1).
    ENETDOWN,

    /// Connection aborted by network (POSIX.1).
    ENETRESET,

    /// Network unreachable (POSIX.1).
    ENETUNREACH,

    /// Too many open files in system (POSIX.1).  On Linux, this is probably a result of encountering the /proc/sys/fs/file-max limit (see proc(5)).
    ENFILE,

    /// No buffer space available (POSIX.1 (XSI STREAMS option)).
    ENOBUFS,

    /// No message is available on the STREAM head read queue (POSIX.1).
    ENODATA,

    /// No such device (POSIX.1).
    ENODEV,

    /// No such file or directory (POSIX.1).
    /// Typically, this error results when a specified,
    /// pathname does not exist, or one of the components in
    /// the directory prefix of a pathname does not exist, or
    /// the specified pathname is a dangling symbolic link.
    ENOENT,


    /// Exec format error (POSIX.1).
    ENOEXEC,

    /// Required key not available.
    ENOKEY,

    /// No locks available (POSIX.1).
    ENOLCK,

    /// Link has been severed (POSIX.1).
    ENOLINK,

    /// No medium found.
    ENOMEDIUM,

    /// Not enough space (POSIX.1).
    ENOMEM,

    /// No message of the desired type (POSIX.1).
    ENOMSG,

    /// Machine is not on the network.
    ENONET,

    /// Package not installed.
    ENOPKG,

    /// Protocol not available (POSIX.1).
    ENOPROTOOPT,

    /// No space left on device (POSIX.1).
    ENOSPC,

    /// No STREAM resources (POSIX.1 (XSI STREAMS option)).
    ENOSR,

    /// Not a STREAM (POSIX.1 (XSI STREAMS option)).
    ENOSTR,

    /// Function not implemented (POSIX.1).
    ENOSYS,

    /// Block device required.
    ENOTBLK,

    /// The socket is not connected (POSIX.1).
    ENOTCONN,

    /// Not a directory (POSIX.1).
    ENOTDIR,

    /// Directory not empty (POSIX.1).
    ENOTEMPTY,

    /// Not a socket (POSIX.1).
    ENOTSOCK,

    /// Operation not supported (POSIX.1).
    ENOTSUP,

    /// Inappropriate I/O control operation (POSIX.1).
    ENOTTY,

    /// Name not unique on network.
    ENOTUNIQ,

    /// No such device or address (POSIX.1).
    ENXIO,

    /// Operation not supported on socket (POSIX.1).
    /// (ENOTSUP and EOPNOTSUPP have the same value on Linux,
    /// but according to POSIX.1 these error values should be
    /// distinct.)
    EOPNOTSUPP,

    /// Value too large to be stored in data type (POSIX.1).
    EOVERFLOW,

    /// Operation not permitted (POSIX.1).
    EPERM,

    /// Protocol family not supported.
    EPFNOSUPPORT,

    /// Broken pipe (POSIX.1).
    EPIPE,

    /// Protocol error (POSIX.1).
    EPROTO,

    /// Protocol not supported (POSIX.1).
    EPROTONOSUPPORT,

    /// Protocol wrong type for socket (POSIX.1).
    EPROTOTYPE,

    /// Result too large (POSIX.1, C99).
    ERANGE,

    /// Remote address changed.
    EREMCHG,

    /// Object is remote.
    EREMOTE,

    /// Remote I/O error.
    EREMOTEIO,

    /// Interrupted system call should be restarted.
    ERESTART,

    /// Read-only filesystem (POSIX.1).
    EROFS,

    /// Cannot send after transport endpoint shutdown.
    ESHUTDOWN,

    /// Invalid seek (POSIX.1).
    ESPIPE,

    /// Socket type not supported.
    ESOCKTNOSUPPORT,

    /// No such process (POSIX.1).
    ESRCH,

    /// Stale file handle (POSIX.1).
    /// This error can occur for NFS and for other
    /// filesystems.
    ESTALE,

    /// Streams pipe error.
    ESTRPIPE,

    /// Timer expired.  (POSIX.1 (XSI STREAMS option))
    /// (POSIX.1 says "STREAM ioctl(2) timeout")
    ETIME,

    /// Connection timed out (POSIX.1).
    ETIMEDOUT,

    /// Text file busy (POSIX.1).
    ETXTBSY,

    /// Structure needs cleaning.
    EUCLEAN,

    /// Protocol driver not attached.
    EUNATCH,

    /// Too many users.
    EUSERS,

    /// Operation would block (may be same value as EAGAIN) (POSIX.1).
    EWOULDBLOCK,

    /// Improper link (POSIX.1).
    EXDEV,

    /// Exchange full.
    EXFULL,
};
