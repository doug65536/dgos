#pragma once

// The <aio.h> header shall define the aiocb structure, which shall include at least the following members:

#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

struct aiocb {
    int             aio_fildes;     // File descriptor.
    off_t           aio_offset;     // File offset.
    volatile void  *aio_buf;        // Location of buffer.
    size_t          aio_nbytes;     // Length of transfer.
    int             aio_reqprio;    // Request priority offset.
    struct sigevent aio_sigevent;   // Signal number and value.
    int             aio_lio_opcode; // Operation to be performed.
};


//
// Return values of cancelation function

// A return value indicating that all requested operations have been canceled.
#define AIO_CANCELED 0

// A return value indicating that some of the requested operations could not be canceled since they are in progress.
#define AIO_NOTCANCELED 1

// A return value indicating that none of the requested operations could be canceled since they are already complete.
#define AIO_ALLDONE 2

//
// Return values of aio_lio_opcode

//
// Opcodes of aio_lio_listio

// A lio_listio() element operation option requesting a read.
#define LIO_READ 0

// A lio_listio() element operation option requesting a write.
#define LIO_WRITE 1

// A lio_listio() element operation option indicating that no transfer
// is requested.
#define LIO_NOP 2

// A lio_listio() synchronization operation indicating that the calling
// thread is to suspend until the lio_listio() operation is complete.
#define LIO_WAIT 0

// A lio_listio() synchronization operation indicating that the calling
// thread is to continue execution while the lio_listio() operation is
// being performed, and no notification is given when the operation is
// complete.
#define LIO_NOWAIT 1

#define AIO_LISTIO_MAX

// The following shall be declared as functions and may also be
// defined as macros. Function prototypes shall be provided.

int      aio_cancel(int, struct aiocb *);

int      aio_error(const struct aiocb *);

int      aio_fsync(int, struct aiocb *);

int      aio_read(struct aiocb *);

ssize_t  aio_return(struct aiocb *);

int      aio_suspend(const struct aiocb * const *, int,
             const struct timespec *);

int      aio_write(struct aiocb *);

int      lio_listio(int, struct aiocb *restrict const * restrict, int,
             struct sigevent *restrict);
