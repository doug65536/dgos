#pragma once

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// The <unistd.h> header shall define the following symbolic constants.
// The values shall be suitable for use in #if preprocessing directives.

// Integer value indicating version of this standard (C-language binding)
// to which the implementation conforms. For implementations conforming
// to POSIX.1-2008, the value shall be 200809L.
#define _POSIX_VERSION

// Integer value indicating version of the Shell and Utilities volume
// of POSIX.1 to which the implementation conforms. For implementations
// conforming to POSIX.1-2008, the value shall be 200809L. For profile
// implementations that define _POSIX_SUBPROFILE
// (see Subprofiling Considerations) in <unistd.h>, _POSIX2_VERSION may
// be left undefined or be defined with the value -1 to indicate that the
// Shell and Utilities volume of POSIX.1 is not supported. In this case,
// a call to sysconf(_SC_2_VERSION) shall return either 200809L or -1
// indicating that the Shell and Utilities volume of POSIX.1 is or is not,
// respectively, supported at runtime.
#define _POSIX2_VERSION

// The <unistd.h> header shall define the following symbolic constant
// only if the implementation supports the XSI option; see XSI Conformance.
// If defined, its value shall be suitable for use in #if preprocessing
// directives.

// Integer value indicating version of the X/Open Portability Guide to
// which the implementation conforms. The value shall be 700.
#define _XOPEN_VERSION

// Constants for Options and Option Groups
// The following symbolic constants, if defined in <unistd.h>,
// shall have a value of -1, 0, or greater, unless otherwise specified below.
// For profile implementations that define _POSIX_SUBPROFILE
// (see Subprofiling Considerations) in <unistd.h>, constants described below
// as always having a value greater than zero need not be defined and,
// if defined, may have a value of -1, 0, or greater.
// The values shall be suitable for use in #if preprocessing directives.

// If a symbolic constant is not defined or is defined with the value -1,
// the option is not supported for compilation. If it is defined with a value
// greater than zero, the option shall always be supported when the application
// is executed. If it is defined with the value zero, the option shall be
// supported for compilation and might or might not be supported at runtime.
// See Options for further information about the conformance requirements of
// these three categories of support.

// The implementation supports the Advisory Information option. If this symbol
// is defined in <unistd.h>, it shall be defined to be -1, 0, or 200809L.
// The value of this symbol reported by sysconf() shall either be
// -1 or 200809L.
#define _POSIX_ADVISORY_INFO

// The implementation supports asynchronous input and output.
// This symbol shall always be set to the value 200809L.
#define _POSIX_ASYNCHRONOUS_IO

// The implementation supports barriers.
// This symbol shall always be set to the value 200809L.
#define _POSIX_BARRIERS

// The use of chown() and fchown() is restricted to a process with appropriate
// privileges, and to changing the group ID of a file only to the effective
// group ID of the process or to one of its supplementary group IDs.
// This symbol shall be defined with a value other than -1.
#define _POSIX_CHOWN_RESTRICTED

// The implementation supports clock selection.
// This symbol shall always be set to the value 200809L.
#define _POSIX_CLOCK_SELECTION

// The implementation supports the Process CPU-Time Clocks option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L.
#define _POSIX_CPUTIME

// The implementation supports the File Synchronization option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L.
#define _POSIX_FSYNC

// The implementation supports the IPv6 option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_IPV6

// The implementation supports job control.
// This symbol shall always be set to a value greater than zero.
#define _POSIX_JOB_CONTROL

// The implementation supports memory mapped Files.
// This symbol shall always be set to the value 200809L.
#define _POSIX_MAPPED_FILES

// The implementation supports the Process Memory Locking option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_MEMLOCK

// The implementation supports the Range Memory Locking option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_MEMLOCK_RANGE

// The implementation supports memory protection.
// This symbol shall always be set to the value 200809L.
#define _POSIX_MEMORY_PROTECTION

// The implementation supports the Message Passing option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_MESSAGE_PASSING

// The implementation supports the Monotonic Clock option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_MONOTONIC_CLOCK

// Pathname components longer than {NAME_MAX} generate an error.
// This symbol shall be defined with a value other than -1.
#define _POSIX_NO_TRUNC

// The implementation supports the Prioritized Input and Output option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_PRIORITIZED_IO

// The implementation supports the Process Scheduling option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_PRIORITY_SCHEDULING

// The implementation supports the Raw Sockets option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_RAW_SOCKETS

// The implementation supports read-write locks.
// This symbol shall always be set to the value 200809L.
#define _POSIX_READER_WRITER_LOCKS

// The implementation supports realtime signals.
// This symbol shall always be set to the value 200809L.
#define _POSIX_REALTIME_SIGNALS

// The implementation supports the Regular Expression Handling option.
// This symbol shall always be set to a value greater than zero.
#define _POSIX_REGEXP

// Each process has a saved set-user-ID and a saved set-group-ID.
// This symbol shall always be set to a value greater than zero.
#define _POSIX_SAVED_IDS

// The implementation supports semaphores.
// This symbol shall always be set to the value 200809L.
#define _POSIX_SEMAPHORES

// The implementation supports the Shared Memory Objects option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_SHARED_MEMORY_OBJECTS

// The implementation supports the POSIX shell.
// This symbol shall always be set to a value greater than zero.
#define _POSIX_SHELL

// The implementation supports the Spawn option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_SPAWN

// The implementation supports spin locks.
// This symbol shall always be set to the value 200809L.
#define _POSIX_SPIN_LOCKS

// The implementation supports the Process Sporadic Server option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_SPORADIC_SERVER

// The implementation supports the Synchronized Input and Output option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_SYNCHRONIZED_IO

// The implementation supports the Thread Stack Address Attribute option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_ATTR_STACKADDR

// The implementation supports the Thread Stack Size Attribute option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_ATTR_STACKSIZE

// The implementation supports the Thread CPU-Time Clocks option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_CPUTIME

// The implementation supports the
// Non-Robust Mutex Priority Inheritance option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_PRIO_INHERIT

// The implementation supports the Non-Robust Mutex Priority Protection option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_PRIO_PROTECT

// The implementation supports the Thread Execution Scheduling option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_PRIORITY_SCHEDULING

// The implementation supports the
// Thread Process-Shared Synchronization option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_PROCESS_SHARED

// The implementation supports the Robust Mutex Priority Inheritance option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT

// The implementation supports the Robust Mutex Priority Protection option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT

// The implementation supports thread-safe functions.
// This symbol shall always be set to the value 200809L.
#define _POSIX_THREAD_SAFE_FUNCTIONS

// The implementation supports the Thread Sporadic Server option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_THREAD_SPORADIC_SERVER

// The implementation supports threads.
// This symbol shall always be set to the value 200809L.
#define _POSIX_THREADS

// The implementation supports timeouts.
// This symbol shall always be set to the value 200809L.
#define _POSIX_TIMEOUTS

// The implementation supports timers.
// This symbol shall always be set to the value 200809L.
#define _POSIX_TIMERS

// The implementation supports the Trace option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_TRACE

// The implementation supports the Trace Event Filter option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_TRACE_EVENT_FILTER

// The implementation supports the Trace Inherit option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_TRACE_INHERIT

// The implementation supports the Trace Log option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_TRACE_LOG

// The implementation supports the Typed Memory Objects option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX_TYPED_MEMORY_OBJECTS

// The implementation provides a C-language compilation environment
// with 32-bit int, long, pointer, and off_t types. [Option End]
#define _POSIX_V6_ILP32_OFF32

// The implementation provides a C-language compilation environment
// with 32-bit int, long, and pointer types and an off_t type using
// at least 64 bits. [Option End]
#define _POSIX_V6_ILP32_OFFBIG

// The implementation provides a C-language compilation environment
// with 32-bit int and 64-bit long, pointer, and off_t types. [Option End]
#define _POSIX_V6_LP64_OFF64

// The implementation provides a C-language compilation environment
// with an int type using at least 32 bits and long, pointer,
// and off_t types using at least 64 bits. [Option End]
#define _POSIX_V6_LPBIG_OFFBIG

// The implementation provides a C-language compilation environment
// with 32-bit int, long, pointer, and off_t types.
#define _POSIX_V7_ILP32_OFF32

// The implementation provides a C-language compilation environment
// with 32-bit int, long, and pointer types and an off_t type using
// at least 64 bits.
#define _POSIX_V7_ILP32_OFFBIG

// The implementation provides a C-language compilation environment
// with 32-bit int and 64-bit long, pointer, and off_t types.
#define _POSIX_V7_LP64_OFF64

// The implementation provides a C-language compilation environment
// with an int type using at least 32 bits and long, pointer, and off_t
// types using at least 64 bits.
#define _POSIX_V7_LPBIG_OFFBIG

// The implementation supports the C-Language Binding option.
// This symbol shall always have the value 200809L.
#define _POSIX2_C_BIND

// The implementation supports the C-Language Development Utilities option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_C_DEV

// The implementation supports the Terminal Characteristics option.
// The value of this symbol reported by sysconf() shall either be
// -1 or a value greater than zero.
#define _POSIX2_CHAR_TERM

// The implementation supports the FORTRAN Development Utilities option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_FORT_DEV

// The implementation supports the FORTRAN Runtime Utilities option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_FORT_RUN

// The implementation supports the creation of locales by
// the localedef utility. If this symbol is defined in <unistd.h>, it shall
// be defined to be -1, 0, or 200809L. The value of this symbol reported by
// sysconf() shall either be -1 or 200809L.
#define _POSIX2_LOCALEDEF

// The implementation supports the
// Batch Environment Services and Utilities option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_PBS

// The implementation supports the Batch Accounting option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_PBS_ACCOUNTING

// The implementation supports the Batch Checkpoint/Restart option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_PBS_CHECKPOINT

// The implementation supports the Locate Batch Job Request option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_PBS_LOCATE

// The implementation supports the Batch Job Message Request option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_PBS_MESSAGE

// The implementation supports the Track Batch Job Request option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_PBS_TRACK

// The implementation supports the Software Development Utilities option.
// If this symbol is defined in <unistd.h>, it shall be defined to be
// -1, 0, or 200809L. The value of this symbol reported by sysconf() shall
// either be -1 or 200809L. [Option End]
#define _POSIX2_SW_DEV

// The implementation supports the
// User Portability Utilities option. If this symbol is defined in <unistd.h>,
// it shall be defined to be -1, 0, or 200809L.
// The value of this symbol reported by sysconf() shall either be
// -1 or 200809L. [Option End]
#define _POSIX2_UPE

// The implementation supports the X/Open Encryption Option Group.
#define _XOPEN_CRYPT

// The implementation supports the Issue 4,
// Version 2 Enhanced Internationalization Option Group.
// This symbol shall always be set to a value other than -1.
#define _XOPEN_ENH_I18N

// The implementation supports the X/Open Realtime Option Group.
#define _XOPEN_REALTIME

// The implementation supports the X/Open Realtime Threads Option Group.
#define _XOPEN_REALTIME_THREADS

// The implementation supports the Issue 4,
// Version 2 Shared Memory Option Group.
// This symbol shall always be set to a value other than -1. [Option End]
#define _XOPEN_SHM

// The implementation supports the XSI STREAMS Option Group. [Option End]
#define _XOPEN_STREAMS

// The implementation supports the XSI option. [Option End]
#define _XOPEN_UNIX

// The implementation supports the UUCP Utilities option. If this symbol
// is defined in <unistd.h>, it shall be defined to be -1, 0, or 200809L.
// The value of this symbol reported by sysconf() shall be either
// -1 or 200809L. [Option End]

// Execution-Time Symbolic Constants

#define _XOPEN_UUCP

// If any of the following symbolic constants are not defined
// in the <unistd.h> header, the value shall vary depending on the file
// to which it is applied. If defined, they shall have values suitable for
// use in #if preprocessing directives.

// If any of the following symbolic constants are defined
// to have value -1 in the <unistd.h> header, the implementation shall
// not provide the option on any file; if any are defined to have a value
// other than -1 in the <unistd.h> header, the implementation shall provide
// the option on all applicable files.

// All of the following values, whether defined as symbolic constants
// in <unistd.h> or not, may be queried with respect to a specific file
// using the pathconf() or fpathconf() functions:


// Asynchronous input or output operations may be performed for the
// associated file.
#define _POSIX_ASYNC_IO

// Prioritized input or output operations may be performed for the
// associated file.
#define _POSIX_PRIO_IO

// Synchronized input or output operations may be performed for the
// associated file.
#define _POSIX_SYNC_IO

// If the following symbolic constants are defined in the <unistd.h> header,
// they apply to files and all paths in all file systems
// on the implementation:

// The resolution in nanoseconds for all file timestamps.
#define _POSIX_TIMESTAMP_RESOLUTION

// Symbolic links can be created.
#define _POSIX2_SYMLINKS

// Constants for Functions


// The <unistd.h> header shall define NULL as described in <stddef.h>.

// The <unistd.h> header shall define the following symbolic constants
// for use with the access() function. The values shall be suitable for
// use in #if preprocessing directives.

// Test for existence of file.
#define F_OK

// Test for read permission.
#define R_OK

// Test for write permission.
#define W_OK

// Test for execute (search) permission.
#define X_OK

// The constants F_OK, R_OK, W_OK, and X_OK and the expressions
// R_OK|W_OK, R_OK|X_OK, and R_OK|W_OK|X_OK shall all have distinct values.

// The <unistd.h> header shall define the following symbolic constants
// for the confstr() function:

// This is the value for the PATH environment variable that finds all
// of the standard utilities that are provided in a manner accessible
// via the exec family of functions.
#define _CS_PATH

// If sysconf(_SC_V7_ILP32_OFF32) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of initial options to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int, long, pointer, and off_t types.
#define _CS_POSIX_V7_ILP32_OFF32_CFLAGS

// If sysconf(_SC_V7_ILP32_OFF32) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of final options to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int, long, pointer, and off_t types.
#define _CS_POSIX_V7_ILP32_OFF32_LDFLAGS

// If sysconf(_SC_V7_ILP32_OFF32) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of libraries to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int, long, pointer, and off_t types.
#define _CS_POSIX_V7_ILP32_OFF32_LIBS

// If sysconf(_SC_V7_ILP32_OFFBIG) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of initial options to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int, long, and pointer types, and an off_t type using
// at least 64 bits.
#define _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS

// If sysconf(_SC_V7_ILP32_OFFBIG) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of final options to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int, long, and pointer types, and an off_t type using
// at least 64 bits.
#define _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS

// If sysconf(_SC_V7_ILP32_OFFBIG) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of libraries to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int, long, and pointer types, and an off_t type using
// at least 64 bits.
#define _CS_POSIX_V7_ILP32_OFFBIG_LIBS

// If sysconf(_SC_V7_LP64_OFF64) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of initial options to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int and 64-bit long, pointer, and off_t types.
#define _CS_POSIX_V7_LP64_OFF64_CFLAGS

// If sysconf(_SC_V7_LP64_OFF64) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of final options to be
// given to the c99 utility to build an application using a programming
// model with 32-bit int and 64-bit long, pointer, and off_t types.
#define _CS_POSIX_V7_LP64_OFF64_LDFLAGS

// If sysconf(_SC_V7_LP64_OFF64) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of libraries to
// be given to the c99 utility to build an application using a programming
// model with 32-bit int and 64-bit long, pointer, and off_t types.
#define _CS_POSIX_V7_LP64_OFF64_LIBS

// If sysconf(_SC_V7_LPBIG_OFFBIG) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of initial options to
// be given to the c99 utility to build an application using a programming
// model with an int type using at least 32 bits and long, pointer, and off_t
// types using at least 64 bits.
#define _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS

// If sysconf(_SC_V7_LPBIG_OFFBIG) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of final options to
// be given to the c99 utility to build an application using a programming
//model with an int type using at least 32 bits and long, pointer, and off_t
// types using at least 64 bits.
#define _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS

// If sysconf(_SC_V7_LPBIG_OFFBIG) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of libraries to
// be given to the c99 utility to build an application using a programming
// model with an int type using at least 32 bits and long, pointer, and off_t
// types using at least 64 bits.
#define _CS_POSIX_V7_LPBIG_OFFBIG_LIBS

// If sysconf(_SC_POSIX_THREADS) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of initial options to
// be given to the c99 utility to build a multi-threaded application.
// These flags are in addition to those associated with any of the
// other _CS_POSIX_V7_*_CFLAGS values used to specify particular type
// size programing environments.
#define _CS_POSIX_V7_THREADS_CFLAGS

// If sysconf(_SC_POSIX_THREADS) returns -1, the meaning of this value
// is unspecified. Otherwise, this value is the set of final options to be
// given to the c99 utility to build a multi-threaded application.
// These flags are in addition to those associated with any of the
// other _CS_POSIX_V7_*_LDFLAGS values used to specify particular type
// size programing environments.
#define _CS_POSIX_V7_THREADS_LDFLAGS

// This value is a <newline>-separated list of names of programming
// environments supported by the implementation in which the widths of
// the blksize_t, cc_t, mode_t, nfds_t, pid_t, ptrdiff_t, size_t, speed_t,
// ssize_t, suseconds_t, tcflag_t, wchar_t, and wint_t types are no greater
// than the width of type long. The format of each name shall be suitable
// for use with the getconf -v option.
#define _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS

// This is the value that provides the environment variable information
// (other than that provided by _CS_PATH) that is required by the
// implementation to create a conforming environment, as described in the
// implementation's conformance documentation.
#define _CS_V7_ENV

#define _CS_POSIX_V6_ILP32_OFF32_CFLAGS
#define _CS_POSIX_V6_ILP32_OFF32_LDFLAGS
#define _CS_POSIX_V6_ILP32_OFF32_LIBS
#define _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS
#define _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS
#define _CS_POSIX_V6_ILP32_OFFBIG_LIBS
#define _CS_POSIX_V6_LP64_OFF64_CFLAGS
#define _CS_POSIX_V6_LP64_OFF64_LDFLAGS
#define _CS_POSIX_V6_LP64_OFF64_LIBS
#define _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS
#define _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS
#define _CS_POSIX_V6_LPBIG_OFFBIG_LIBS
#define _CS_POSIX_V6_WIDTH_RESTRICTED_ENVS
#define _CS_V6_ENV

// The <unistd.h> header shall define SEEK_CUR, SEEK_END, and SEEK_SET
// as described in <stdio.h>.

// [XSI] [Option Start] The <unistd.h> header shall define the following
// symbolic constants as possible values for the function argument to
// the lockf() function:

// Lock a section for exclusive use.
#define F_LOCK

// Test section for locks by other processes.
#define F_TEST

// Test and lock a section for exclusive use.
#define F_TLOCK

// Unlock locked sections.
#define F_ULOCK

// The <unistd.h> header shall define the following symbolic
// constants for pathconf():


#define _PC_2_SYMLINKS
#define _PC_ALLOC_SIZE_MIN
#define _PC_ASYNC_IO
#define _PC_CHOWN_RESTRICTED
#define _PC_FILESIZEBITS
#define _PC_LINK_MAX
#define _PC_MAX_CANON
#define _PC_MAX_INPUT
#define _PC_NAME_MAX
#define _PC_NO_TRUNC
#define _PC_PATH_MAX
#define _PC_PIPE_BUF
#define _PC_PRIO_IO
#define _PC_REC_INCR_XFER_SIZE
#define _PC_REC_MAX_XFER_SIZE
#define _PC_REC_MIN_XFER_SIZE
#define _PC_REC_XFER_ALIGN
#define _PC_SYMLINK_MAX
#define _PC_SYNC_IO
#define _PC_TIMESTAMP_RESOLUTION
#define _PC_VDISABLE

// The <unistd.h> header shall define the following symbolic
// constants for sysconf():

#define _SC_2_C_BIND
#define _SC_2_C_DEV
#define _SC_2_CHAR_TERM
#define _SC_2_FORT_DEV
#define _SC_2_FORT_RUN
#define _SC_2_LOCALEDEF
#define _SC_2_PBS
#define _SC_2_PBS_ACCOUNTING
#define _SC_2_PBS_CHECKPOINT
#define _SC_2_PBS_LOCATE
#define _SC_2_PBS_MESSAGE
#define _SC_2_PBS_TRACK
#define _SC_2_SW_DEV
#define _SC_2_UPE
#define _SC_2_VERSION
#define _SC_ADVISORY_INFO
#define _SC_AIO_LISTIO_MAX
#define _SC_AIO_MAX
#define _SC_AIO_PRIO_DELTA_MAX
#define _SC_ARG_MAX
#define _SC_ASYNCHRONOUS_IO
#define _SC_ATEXIT_MAX
#define _SC_BARRIERS
#define _SC_BC_BASE_MAX
#define _SC_BC_DIM_MAX
#define _SC_BC_SCALE_MAX
#define _SC_BC_STRING_MAX
#define _SC_CHILD_MAX
#define _SC_CLK_TCK
#define _SC_CLOCK_SELECTION
#define _SC_COLL_WEIGHTS_MAX
#define _SC_CPUTIME
#define _SC_DELAYTIMER_MAX
#define _SC_EXPR_NEST_MAX
#define _SC_FSYNC
#define _SC_GETGR_R_SIZE_MAX
#define _SC_GETPW_R_SIZE_MAX
#define _SC_HOST_NAME_MAX
#define _SC_IOV_MAX
#define _SC_IPV6
#define _SC_JOB_CONTROL
#define _SC_LINE_MAX
#define _SC_LOGIN_NAME_MAX
#define _SC_MAPPED_FILES
#define _SC_MEMLOCK
#define _SC_MEMLOCK_RANGE
#define _SC_MEMORY_PROTECTION
#define _SC_MESSAGE_PASSING
#define _SC_MONOTONIC_CLOCK
#define _SC_MQ_OPEN_MAX
#define _SC_MQ_PRIO_MAX
#define _SC_NGROUPS_MAX
#define _SC_OPEN_MAX
#define _SC_PAGE_SIZE
#define _SC_PAGESIZE
#define _SC_PRIORITIZED_IO
#define _SC_PRIORITY_SCHEDULING
#define _SC_RAW_SOCKETS
#define _SC_RE_DUP_MAX
#define _SC_READER_WRITER_LOCKS
#define _SC_REALTIME_SIGNALS
#define _SC_REGEXP
#define _SC_RTSIG_MAX
#define _SC_SAVED_IDS
#define _SC_SEM_NSEMS_MAX
#define _SC_SEM_VALUE_MAX
#define _SC_SEMAPHORES
#define _SC_SHARED_MEMORY_OBJECTS
#define _SC_SHELL
#define _SC_SIGQUEUE_MAX
#define _SC_SPAWN
#define _SC_SPIN_LOCKS
#define _SC_SPORADIC_SERVER
#define _SC_SS_REPL_MAX
#define _SC_STREAM_MAX
#define _SC_SYMLOOP_MAX
#define _SC_SYNCHRONIZED_IO
#define _SC_THREAD_ATTR_STACKADDR
#define _SC_THREAD_ATTR_STACKSIZE
#define _SC_THREAD_CPUTIME
#define _SC_THREAD_DESTRUCTOR_ITERATIONS
#define _SC_THREAD_KEYS_MAX
#define _SC_THREAD_PRIO_INHERIT
#define _SC_THREAD_PRIO_PROTECT
#define _SC_THREAD_PRIORITY_SCHEDULING
#define _SC_THREAD_PROCESS_SHARED
#define _SC_THREAD_ROBUST_PRIO_INHERIT
#define _SC_THREAD_ROBUST_PRIO_PROTECT
#define _SC_THREAD_SAFE_FUNCTIONS
#define _SC_THREAD_SPORADIC_SERVER
#define _SC_THREAD_STACK_MIN
#define _SC_THREAD_THREADS_MAX
#define _SC_THREADS
#define _SC_TIMEOUTS
#define _SC_TIMER_MAX
#define _SC_TIMERS
#define _SC_TRACE
#define _SC_TRACE_EVENT_FILTER
#define _SC_TRACE_EVENT_NAME_MAX
#define _SC_TRACE_INHERIT
#define _SC_TRACE_LOG
#define _SC_TRACE_NAME_MAX
#define _SC_TRACE_SYS_MAX
#define _SC_TRACE_USER_EVENT_MAX
#define _SC_TTY_NAME_MAX
#define _SC_TYPED_MEMORY_OBJECTS
#define _SC_TZNAME_MAX
#define _SC_V7_ILP32_OFF32
#define _SC_V7_ILP32_OFFBIG
#define _SC_V7_LP64_OFF64
#define _SC_V7_LPBIG_OFFBIG
#define _SC_V6_ILP32_OFF32
#define _SC_V6_ILP32_OFFBIG
#define _SC_V6_LP64_OFF64
#define _SC_V6_LPBIG_OFFBIG
#define _SC_VERSION
#define _SC_XOPEN_CRYPT
#define _SC_XOPEN_ENH_I18N
#define _SC_XOPEN_REALTIME
#define _SC_XOPEN_REALTIME_THREADS
#define _SC_XOPEN_SHM
#define _SC_XOPEN_STREAMS
#define _SC_XOPEN_UNIX
#define _SC_XOPEN_UUCP
#define _SC_XOPEN_VERSION

// The two constants _SC_PAGESIZE and _SC_PAGE_SIZE may be defined to
// have the same value.

// The <unistd.h> header shall define the following symbolic constants
// for file streams:

// File number of stderr; 2.
#define STDERR_FILENO	2

// File number of stdin; 0.
#define STDIN_FILENO	0

// File number of stdout; 1.
#define STDOUT_FILENO	1

// The <unistd.h> header shall define the following symbolic constant
// for terminal special character handling:

// This symbol shall be defined to be the value of a character that shall
// disable terminal special character handling as described in
// Special Control Characters.
// This symbol shall always be set to a value other than -1.
#define _POSIX_VDISABLE

// Type Definitions

// The <unistd.h> header shall define the
// size_t, ssize_t, uid_t, gid_t, off_t, and pid_t types
// as described in <sys/types.h>.

// The <unistd.h> header shall define the intptr_t type
// as described in <stdint.h>.

// Declarations
// The following shall be declared as functions and may also be defined
// as macros. Function prototypes shall be provided.

int          access(const char *, int);
unsigned     alarm(unsigned);
int          chdir(const char *);
int          chown(const char *, uid_t, gid_t);
int          close(int);
size_t       confstr(int, char *, size_t);

char        *crypt(const char *, const char *);

int          dup(int);


int          dup2(int, int);
void         _exit(int);

void         encrypt(char [64], int);

int          execl(const char *, const char *, ...);
int          execle(const char *, const char *, ...);
int          execlp(const char *, const char *, ...);
int          execv(const char *, char *const []);
int          execve(const char *, char *const [], char *const []);
int          execvp(const char *, char *const []);
int          faccessat(int, const char *, int, int);
int          fchdir(int);
int          fchown(int, uid_t, gid_t);
int          fchownat(int, const char *, uid_t, gid_t, int);

int          fdatasync(int);

int          fexecve(int, char *const [], char *const []);
pid_t        fork(void);
long         fpathconf(int, int);

int          fsync(int);

int          ftruncate(int, off_t);
char        *getcwd(char *, size_t);
gid_t        getegid(void);
uid_t        geteuid(void);
gid_t        getgid(void);
int          getgroups(int, gid_t*);

long         gethostid(void);

int          gethostname(char *, size_t);
char        *getlogin(void);
int          getlogin_r(char *, size_t);
int          getopt(int, char * const [], const char *);
pid_t        getpgid(pid_t);
pid_t        getpgrp(void);
pid_t        getpid(void);
pid_t        getppid(void);
pid_t        getsid(pid_t);
uid_t        getuid(void);
int          isatty(int);
int          lchown(const char *, uid_t, gid_t);
int          link(const char *, const char *);
int          linkat(int, const char *, int, const char *, int);

int          lockf(int, int, off_t);

off_t        lseek(int, off_t, int);

int          nice(int);

long         pathconf(const char *, int);
int          pause(void);
int          pipe(int *fds);
ssize_t      pread(int, void *, size_t, off_t);
ssize_t      pwrite(int, const void *, size_t, off_t);
ssize_t      read(int, void *, size_t);
ssize_t      readlink(const char *restrict, char *restrict, size_t);
ssize_t      readlinkat(int, const char *restrict, char *restrict, size_t);
int          rmdir(const char *);
int          setegid(gid_t);
int          seteuid(uid_t);
int          setgid(gid_t);


int          setpgid(pid_t, pid_t);

pid_t        setpgrp(void);

int          setregid(gid_t, gid_t);
int          setreuid(uid_t, uid_t);

pid_t        setsid(void);
int          setuid(uid_t);
unsigned     sleep(unsigned);

void         swab(const void *restrict, void *restrict, ssize_t);

int          symlink(const char *, const char *);
int          symlinkat(const char *, int, const char *);

void         sync(void);

long         sysconf(int);
pid_t        tcgetpgrp(int);
int          tcsetpgrp(int, pid_t);
int          truncate(const char *, off_t);
char        *ttyname(int);
int          ttyname_r(int, char *, size_t);
int          unlink(const char *);
int          unlinkat(int, const char *, int);
ssize_t      write(int, const void *, size_t);

// Implementations may also include the pthread_atfork() prototype
// as defined in <pthread.h>. Implementations may also include the
// ctermid() prototype as defined in <stdio.h>.

// The <unistd.h> header shall declare the following external variables:

extern char  *optarg;
extern int    opterr, optind, optopt;
