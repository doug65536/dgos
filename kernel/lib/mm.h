#pragma once

#include "types.h"
#include "cpu/cpu_metrics.h"
//#include "process.h"
#include "errno.h"
#include "string.h"
#include "refcount.h"

__BEGIN_DECLS

/// Failure return value for memory mapping functions
#define MAP_FAILED ((void*)-1)

///
/// mmap __prot

#define PROT_NONE           0x00
#define PROT_READ           0x01
#define PROT_WRITE          0x02
#define PROT_EXEC           0x04

///
/// mmap __flags

/// Map at specified address, otherwise, just fail
#define MAP_FIXED           0x00000001

/// Lock the mapped region in memory
#define MAP_LOCKED          0x00000002

/// Hint that pages will be used as stacks
#define MAP_STACK           0x00000004

/// Mapping grows downward in memory
#define MAP_GROWSDOWN       0x00000008

/// Use huge pages
#define MAP_HUGETLB         0x00000010

/// Don't commit swap space for uncommitted pages,
/// just SIGSEGV if an uncommitted page is touched
/// when there is insufficient space
#define MAP_NORESERVE       0x00000020

/// Don't do demand paging, actually commit pages up front
/// Hint file mappings to read ahead
#define MAP_POPULATE        0x00000040

/// If allowed, skip clearing the pages to zero
#define MAP_UNINITIALIZED   0x00000080

/// Map below 2GB
#define MAP_32BIT           0x00000100

/// Not file backed
#define MAP_ANONYMOUS       0x00000200

// Undefined flag mask
#define MAP_INVALID_MASK    0x001FF800

// Allowed in user mode
#define MAP_USER_MASK       0x000003FF

// Kernel only: Commit no pages
#define MAP_NOCOMMIT        0x00200000

// Kernel only: Exclusive (fail if address range is not free)
#define MAP_EXCLUSIVE       0x00400000

// Kernel only: Global
#define MAP_GLOBAL          0x00800000

/// Kernel only: Map near the kernel (modules)
#define MAP_NEAR            0x01000000

/// Kernel only: Map user mode pages
#define MAP_USER            0x02000000

/// Kernel only: Request weakly ordered memory
#define MAP_WEAKORDER       0x04000000

/// Kernel only: Map a device mapping
#define MAP_DEVICE          0x08000000

/// Kernel only: Write through
#define MAP_WRITETHRU       0x10000000

/// Kernel only: Disable caching
#define MAP_NOCACHE         0x20000000

/// Kernel only: The provided address is a physical address
#define MAP_PHYSICAL        0x40000000

/// Ignored. Redundant.
#define MAP_DENYWRITE       0
#define MAP_EXECUTABLE      0
#define MAP_FILE            0
#define MAP_NONBLOCK        0

/// Allocate address space only
KERNEL_API void *mm_alloc_space(size_t size);

/// Map a range of address space
/// __addr, hint, map memory as near as possible to that address,
/// unless MAP_FIXED is used
/// __len, size, in bytes
/// __prot, bitmask, PROT_EXEC PROT_READ PROT_WRITE PROT_NONE
/// __flags, bitmask, MAP_*
/// __fd, file descriptor, file backed mapping
/// __offset, file pointer, position in file to start mapping
KERNEL_API void *mmap(void *__addr,
        size_t __len,
        int __prot,
        int __flags,
        int __fd = -1,
        off_t __offset = 0);

/// Unmap a range of address space
/// Unmap the range from __addr to __len
KERNEL_API int munmap(void *__addr,
        size_t size);

/// Permit a mapping to be moved to a new address
/// Without this flag, mremap fails if it cannot be resized
#define MREMAP_MAYMOVE      0x00000002

/// Move the mapping to the specified __new_address
/// Fail if not possible
#define MREMAP_FIXED        0x00000001

#define MREMAP_INVALID_MASK 0xFFFFFFFC

/// Move and/or resize a range of address space
/// __old_address, address of mapping to be remapped
/// __old_size, the quantity of address space to be moved
/// __new_size, the quantity of address space needed after the move
/// __flags, bitmaps, MREMAP_*
/// __new_address, ignored unless MREMAP_FIXED was specified
KERNEL_API void *mremap(void *__old_address,
        size_t __old_size,
        size_t __new_size,
        int __flags,
        void *__new_address = nullptr,
        errno_t *ret_errno = nullptr);

/// Change protection on a range of memory
/// __addr, address, start of range
/// __len, bytes, length of range
/// __prot, bitfield, PROT_*
KERNEL_API int mprotect(void *__addr, size_t __len, int __prot);

/// Advise normal behavior
#define MADV_NORMAL         (0)

/// Advise that read-ahead is probably useful
#define MADV_SEQUENTIAL     (1)

/// Advise that read-ahead probably won't help
#define MADV_RANDOM         (-1)

/// Advise that this range is going to be needed soon
#define MADV_WILLNEED       (2)

/// Advise that this region can be discarded, and replaced
/// with zero pages or reread from the backing file if it
/// is touched again
#define MADV_DONTNEED       (-2)

/// Advise that this region can be discarded and deleted
/// from the backing file
#define MADV_REMOVE         (7)

/// Advise that this region should be cloned when forking
#define MADV_DOFORK         (3)

/// Advise that this region should not be cloned when forking
#define MADV_DONTFORK       (-3)

/// Advise that there is something wrong with the RAM in this region
#define MADV_HWPOISON       (8)

/// Advise that we should pretend that there is something wrong with
/// the RAM in this region
#define MADV_SOFT_OFFLINE   (-8)

/// Advise that this page should be considered in background page
/// deduplication
#define MADV_MERGEABLE      (4)

/// Advise that this page should not be considered in background page
/// deduplication
#define MADV_UNMERGEABLE    (-4)

/// Advise that this region is a good candidate for huge pages
#define MADV_HUGEPAGE       (5)

/// Advise that this region is not a good candidate for huge pages
#define MADV_NOHUGEPAGE     (-5)

// Advise that this region is probably useful in core dumps
#define MADV_DODUMP         (6)

/// Advise that this region is probably not useful in core dumps
#define MADV_DONTDUMP       (-6)

// Extension: Advise that this region can use weak ordering
#define MADV_WEAKORDER      (9)

// Extension: Advise that this region cannot use weak ordering
#define MADV_STRONGORDER    (-9)

#define MADV_UNINITIALIZED  0x80

/// Set hints about a range of address space
/// __addr, address, start of range
/// __len, size, of range
/// __advice, bitfield, MADV_*
KERNEL_API int madvise(void *__addr, size_t __len, int __advice);

/// Lock a range of address space in memory
int mlock(void const *__addr, size_t __len);

/// Unlock a range of address space
int munlock(void const *__addr, size_t __len);

/// Ensure a range of address space is written back to the device
KERNEL_API int msync(void const *__addr, size_t __len, int __flags);

/// msync flags
#define MS_ASYNC        0
#define MS_SYNC         1
#define MS_INVALIDATE   0

/// Return the physical address for the specified linear address
/// Page faults if the memory region is not mapped
KERNEL_API uintptr_t mphysaddr(volatile void const *addr);

struct mmphysrange_t {
    uintptr_t physaddr;
    size_t size;
};

/// Fill in an array of physical memory ranges corresponding to
/// the specified range of linear address space.
/// If ranges is null, returns the number of entries it would need.
/// ranges_count limits the number of ranges returned.
/// If any part of the linear address range is not mapped
/// then page fault.
/// max_size limits the size of an individual range,
/// if necessary a large contiguous range of physical
/// addresses will be split into smaller ranges with
/// sizes less than or equal to max_length
KERNEL_API size_t mphysranges(mmphysrange_t *ranges,
                              size_t ranges_count,
                              void const *addr, size_t size,
                              size_t max_size);

// Iterate through the ranges and add up the size
KERNEL_API size_t mphysranges_total(mmphysrange_t *ranges,
                                    size_t ranges_count);

// Ensure no range crosses the specified boundary
// For example, pass 16 in log2_boundary to modify
// the region list to never cross a 64KB boundary
KERNEL_API bool mphysranges_split(mmphysrange_t *ranges, size_t &ranges_count,
                                  size_t count_limit, uint8_t log2_boundary);

// Return true if the address is present
bool mpresent(uintptr_t addr, size_t size);

/// Lock all process address space in memory
#define MCL_CURRENT

/// Lock future address space operations in memory
#define MCL_FUTURE

/// Lock all process memory
int mlockall(int __flags);

/// Unlock all process memory
int munlockall(void);

/// Set the program break to the specified value
int brk(void *__addr);

/// Adjust the program break by specified value
void *sbrk(intptr_t __increment);

/// Absolute minimum limits
#ifndef _POSIX_ARG_MAX
#define _POSIX_ARG_MAX (4096)
#define _POSIX_CHILD_MAX (25)
#define _POSIX_HOST_NAME_MAX (255)
#define _POSIX_LOGIN_NAME_MAX (9)
#define _POSIX_OPEN_MAX (20)
#define _POSIX2_RE_DUP_MAX (255)
#define _POSIX_STREAM_MAX (8)
#define _POSIX_SYMLOOP_MAX (8)
#define _POSIX_TTY_NAME_MAX (9)
#define _POSIX_TZNAME_MAX (6)
#endif

/// Maximum number of arguments to exec functions
//#define _SC_ARG_MAX
//#define ARG_MAX _SC_ARG_MAX

/// Maximum number of processes per user
#define _SC_CHILD_MAX
#define CHILD_MAX _SC_CHILD_MAX

/// Maximum length of host name, not including null
#define _SC_HOST_NAME_MAX
#define HOST_NAME_MAX _SC_HOST_NAME_MAX

/// Maximum length of a login name
#define _SC_LOGIN_NAME_MAX
#define LOGIN_NAME_MAX _SC_LOGIN_NAME_MAX

/// Number of clock ticks per second. Must be 1000000
#define _SC_CLK_TCK

/// Maximum number of open files
#define _SC_OPEN_MAX
#define OPEN_MAX _SC_OPEN_MAX

/// Size of page in bytes. Must be >= 1
#define _SC_PAGESIZE    4096
#define PAGESIZE _SC_PAGESIZE

/// The number of repeated occurrences of a BRE premitted by regex and regcomp
#define _SC_RE_DUP_MAX
#define RE_DUP_MAX _SC_RE_DUP_MAX

/// Maximum number of streams that a process can have open
#define _SC_STREAM_MAX
#define STREAM_MAX _SC_STREAM_MAX

/// Maximum number of symbolic links to permit in a path resolution
#define _SC_SYMLOOP_MAX
#define SYMLOOP_MAX _SC_SYMLOOP_MAX

/// Maximum length of a terminal device name
#define _SC_TTY_NAME_MAX
#define TTY_NAME_MAX _SC_TTY_NAME_MAX

/// Maximum length of a timezone name
#define _SC_TZNAME_MAX
#define TZNAME_MAX _SC_TZNAME_MAX

/// Year and month of POSIX revision 199009L
#define _SC_VERSION
#define _POSIX_VERSION _SC_VERSION

/// The number of physical pages of memory
#define _SC_PHYS_PAGES

/// The number of available physical pages of memory
#define _SC_AVPHYS_PAGES

/// The number of processors
#define _SC_NPROCESSORS_CONF

/// The number of online processors
#define _SC_NPROCESSORS_ONLN

/// Query system configuration
long sysconf(int __name);

typedef int (*mm_dev_mapping_callback_t)(
        void *context, void *base_addr,
        uint64_t offset, uint64_t length, bool read, bool flush);

KERNEL_API void *mmap_register_device(void *context,
                                      uint64_t block_size,
                                      uint64_t block_count,
                                      int prot,
                                      mm_dev_mapping_callback_t callback,
                                      void *addr = nullptr);

// Allocate/free contiguous physical memory
KERNEL_API uintptr_t mm_alloc_contiguous(size_t size);
KERNEL_API void mm_free_contiguous(uintptr_t addr, size_t size);

// Allocate/free memory hole (for I/O devices)
KERNEL_API uintptr_t mm_alloc_hole(size_t size);
KERNEL_API void mm_free_hole(uintptr_t addr, size_t size);

uintptr_t mm_new_process(process_t *process, bool use64);

KERNEL_API void *mmap_window(size_t size);
KERNEL_API void munmap_window(void *addr, size_t size);
KERNEL_API int alias_window(void *addr, size_t size,
                 mmphysrange_t const *ranges, size_t range_count);

void mm_init_process(process_t *process, bool use64);

uintptr_t mm_fork_kernel_text();

void mm_set_master_pagedir();

__END_DECLS

#include "cxxexception.h"

class inval_user_pf : public ext::exception {
    inval_user_pf(uintptr_t address, bool insn, bool write);

    inval_user_pf(inval_user_pf const&) = default;
    ~inval_user_pf() = default;

    uintptr_t const address = 0;
    bool insn = false;
    bool write = false;

    // exception interface
protected:
    char const *what() const noexcept override;
};

class ralloc_item_t : refcounted<ralloc_item_t> {
public:
    virtual ~ralloc_item_t() = 0;
};

class ralloc_t {
public:

private:
};

_use_result
uint_least64_t mm_memory_remaining();
_use_result
uint_least64_t mm_get_free_page_count();
_use_result
uint_least64_t mm_get_phys_mem_size();
