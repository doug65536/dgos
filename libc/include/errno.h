#pragma once

#define __ERRNO_DIRECT_TLS  1

typedef int errno_t;

#if __ERRNO_DIRECT_TLS
extern __thread errno_t errno;
#else

//extern "C" int *__errno_location();
//#define errno (*__errno_location())

#endif

/// Argument list too long (POSIX.1).
#define E2BIG 1

/// Permission denied (POSIX.1).
#define EACCES 2

/// Address already in use (POSIX.1).
#define EADDRINUSE 3

/// Address not available (POSIX.1).
#define EADDRNOTAVAIL 4

/// Address family not supported (POSIX.1).
#define EAFNOSUPPORT 5

/// Resource temporarily unavailable
/// (may be the same value as EWOULDBLOCK) (POSIX.1).
#define EAGAIN 6

/// Connection already in progress (POSIX.1).
#define EALREADY 7

/// Invalid exchange.
#define EBADE 8

/// Bad file descriptor (POSIX.1).
#define EBADF 9

/// File descriptor in bad state.
#define EBADFD 10

/// Bad message (POSIX.1).
#define EBADMSG 11

/// Invalid request descriptor.
#define EBADR 12

/// Invalid request code.
#define EBADRQC 13

/// Invalid slot.
#define EBADSLT 14

/// Device or resource busy (POSIX.1).
#define EBUSY 15

/// Operation canceled (POSIX.1).
#define ECANCELED 16

/// No child processes (POSIX.1).
#define ECHILD 17

/// Channel number out of range.
#define ECHRNG 18

/// Communication error on send.
#define ECOMM 19

/// Connection aborted (POSIX.1).
#define ECONNABORTED 20

/// Connection refused (POSIX.1).
#define ECONNREFUSED 21

/// Connection reset (POSIX.1).
#define ECONNRESET 22

/// Resource deadlock avoided (POSIX.1).
#define EDEADLK 23

/// Synonym for EDEADLK.
#define EDEADLOCK EDEADLK

/// Destination address required (POSIX.1).
#define EDESTADDRREQ 24

/// Mathematics argument out of domain of function (POSIX.1, C99).
#define EDOM 25

/// Disk quota exceeded (POSIX.1).
#define EDQUOT 26

/// File exists (POSIX.1).
#define EEXIST 27

/// Bad address (POSIX.1).
#define EFAULT 28

/// File too large (POSIX.1).
#define EFBIG 29

/// Host is down.
#define EHOSTDOWN 30

/// Host is unreachable (POSIX.1).
#define EHOSTUNREACH 31

/// Identifier removed (POSIX.1).
#define EIDRM 32

/// Invalid or incomplete multibyte or wide character (POSIX.1, C99).
#define EILSEQ 33

/// Operation in progress (POSIX.1).
#define EINPROGRESS 34

/// Interrupted function call (POSIX.1); see signal(7).
#define EINTR 35

/// Invalid argument (POSIX.1).
#define EINVAL 36

/// Input/output error (POSIX.1).
#define EIO 37

/// Socket is connected (POSIX.1).
#define EISCONN 38

/// Is a directory (POSIX.1).
#define EISDIR 39

/// Is a named type file.
#define EISNAM 40

/// Key has expired.
#define EKEYEXPIRED 41

/// Key was rejected by service.
#define EKEYREJECTED 42

/// Key has been revoked.
#define EKEYREVOKED 43

/// Level 2 halted.
#define EL2HLT 44

/// Level 2 not synchronized.
#define EL2NSYNC 45

/// Level 3 halted.
#define EL3HLT 46

/// Level 3 halted.
#define EL3RST 47

/// Cannot access a needed shared library.
#define ELIBACC 48

/// Accessing a corrupted shared library.
#define ELIBBAD 49

/// Attempting to link in too many shared libraries.
#define ELIBMAX 50

/// .lib section in a.out corrupted
#define ELIBSCN 51

/// Cannot exec a shared library directly.
#define ELIBEXEC 52

/// Too many levels of symbolic links (POSIX.1).
#define ELOOP 53

/// Wrong medium type.
#define EMEDIUMTYPE 54

/// Too many open files (POSIX.1).
/// Commonly caused by exceeding the RLIMIT_NOFILE resource limit
/// described in getrlimit(2).
#define EMFILE 55

/// Too many links (POSIX.1).
#define EMLINK 56

/// Message too long (POSIX.1).
#define EMSGSIZE 57

/// Multihop attempted (POSIX.1).
#define EMULTIHOP 58

/// Filename too long (POSIX.1).
#define ENAMETOOLONG 59

/// Network is down (POSIX.1).
#define ENETDOWN 60

/// Connection aborted by network (POSIX.1).
#define ENETRESET 61

/// Network unreachable (POSIX.1).
#define ENETUNREACH 62

/// Too many open files in system (POSIX.1).
/// On Linux, this is probably a result of encountering the
///  /proc/sys/fs/file-max limit (see proc(5)).
#define ENFILE 63

/// No buffer space available (POSIX.1 (XSI STREAMS option)).
#define ENOBUFS 64

/// No message is available on the STREAM head read queue (POSIX.1).
#define ENODATA 65

/// No such device (POSIX.1).
#define ENODEV 66

/// No such file or directory (POSIX.1).
/// Typically, this error results when a specified,
/// pathname does not exist, or one of the components in
/// the directory prefix of a pathname does not exist, or
/// the specified pathname is a dangling symbolic link.
#define ENOENT 67

/// Exec format error (POSIX.1).
#define ENOEXEC 68

/// Required key not available.
#define ENOKEY 69

/// No locks available (POSIX.1).
#define ENOLCK 70

/// Link has been severed (POSIX.1).
#define ENOLINK 71

/// No medium found.
#define ENOMEDIUM 72

/// Not enough space (POSIX.1).
#define ENOMEM 73

/// No message of the desired type (POSIX.1).
#define ENOMSG 74

/// Machine is not on the network.
#define ENONET 75

/// Package not installed.
#define ENOPKG 76

/// Protocol not available (POSIX.1).
#define ENOPROTOOPT 77

/// No space left on device (POSIX.1).
#define ENOSPC 78

/// No STREAM resources (POSIX.1 (XSI STREAMS option)).
#define ENOSR 79

/// Not a STREAM (POSIX.1 (XSI STREAMS option)).
#define ENOSTR 80

/// Function not implemented (POSIX.1).
#define ENOSYS 81

/// Block device required.
#define ENOTBLK 82

/// The socket is not connected (POSIX.1).
#define ENOTCONN 83

/// Not a directory (POSIX.1).
#define ENOTDIR 84

/// Directory not empty (POSIX.1).
#define ENOTEMPTY 85

/// Not a socket (POSIX.1).
#define ENOTSOCK 86

/// Operation not supported (POSIX.1).
#define ENOTSUP 87

/// Inappropriate I/O control operation (POSIX.1).
#define ENOTTY 88

/// Name not unique on network.
#define ENOTUNIQ 89

/// No such device or address (POSIX.1).
#define ENXIO 90

/// Operation not supported on socket (POSIX.1).
/// (ENOTSUP and EOPNOTSUPP have the same value on Linux,
/// but according to POSIX.1 these error values should be
/// distinct.)
#define EOPNOTSUPP 91

/// Value too large to be stored in data type (POSIX.1).
#define EOVERFLOW 92

/// Operation not permitted (POSIX.1).
#define EPERM 93

/// Protocol family not supported.
#define EPFNOSUPPORT 94

/// Broken pipe (POSIX.1).
#define EPIPE 95

/// Protocol error (POSIX.1).
#define EPROTO 96

/// Protocol not supported (POSIX.1).
#define EPROTONOSUPPORT 97

/// Protocol wrong type for socket (POSIX.1).
#define EPROTOTYPE 98

/// Result too large (POSIX.1, C99).
#define ERANGE 99

/// Remote address changed.
#define EREMCHG 100

/// Object is remote.
#define EREMOTE 101

/// Remote I/O error.
#define EREMOTEIO 102

/// Interrupted system call should be restarted.
#define ERESTART 103

/// Read-only filesystem (POSIX.1).
#define EROFS 104

/// Cannot send after transport endpoint shutdown.
#define ESHUTDOWN 105

/// Invalid seek (POSIX.1).
#define ESPIPE 106

/// Socket type not supported.
#define ESOCKTNOSUPPORT 107

/// No such process (POSIX.1).
#define ESRCH 108

/// Stale file handle (POSIX.1).
/// This error can occur for NFS and for other
/// filesystems.
#define ESTALE 109

/// Streams pipe error.
#define ESTRPIPE 110

/// Timer expired.  (POSIX.1 (XSI STREAMS option))
/// (POSIX.1 says "STREAM ioctl(2) timeout")
#define ETIME 111

/// Connection timed out (POSIX.1).
#define ETIMEDOUT 112

/// Text file busy (POSIX.1).
#define ETXTBSY 113

/// Structure needs cleaning.
#define EUCLEAN 114

/// Protocol driver not attached.
#define EUNATCH 115

/// Too many users.
#define EUSERS 116

/// Operation would block (may be same value as EAGAIN) (POSIX.1).
#define EWOULDBLOCK 117

/// Improper link (POSIX.1).
#define EXDEV 118

/// Exchange full.
#define EXFULL 119

// Extensions

// Unable to accept/provide data at the required rate
#define EOVERLOAD 120

// Insufficient bandwidth
#define EBANDWIDTH 121

// Stopped
#define ESTOPPED 122

// Did not receive expected amount of data
#define ESHORT 123

#define _MAX_ERRNO 124
