#pragma once
#include "types.h"
#include "dirent.h"

#include "vector.h"
#include "cxxstring.h"
#include "syscall/sys_limits.h"
#include "sys/sys_types.h"

#define SEEK_CUR    0
#define SEEK_SET    1
#define SEEK_END    2
#define SEEK_DATA   3
#define SEEK_HOLE   4

// fcntl.h constants

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

//
// sys_flock 'what'

#define LOCK_EX 0
#define LOCK_SH 1
#define LOCK_UN 2
#define LOCK_NB 4

//
// flock::l_type

// Unlock.
#define F_UNLCK 1

// Shared or read lock.
#define F_RDLCK 2

// Exclusive or write lock.
#define F_WRLCK 3

struct flock {
    // Type of lock; F_RDLCK, F_WRLCK, F_UNLCK.
    short l_type;

    // Flag for starting offset.
    short l_whence;

    // 4 bytes of asinine padding here because
    // original author apparently "knew" off_t was 32 bits

    // Relative offset in bytes.
    off_t l_start;

    // Size; if 0 then until EOF.
    off_t l_len;

    pid_t l_pid;
};

struct path_t;

__BEGIN_DECLS

bool file_ref_filetab(int id);

KERNEL_API int file_creatat(int dirid, char const *path, mode_t mode);
KERNEL_API int file_openat(int dirid, char const* path,
                           int flags, mode_t mode = 0);
KERNEL_API int file_close(int id);
KERNEL_API ssize_t file_read(int id, void *buf, size_t bytes);
KERNEL_API ssize_t file_write(int id, void const *buf, size_t bytes);
KERNEL_API ssize_t file_pread(int id, void *buf, size_t bytes, off_t ofs);
KERNEL_API ssize_t file_pwrite(int id, void const *buf,
                               size_t bytes, off_t ofs);
KERNEL_API off_t file_seek(int id, off_t ofs, int whence);
KERNEL_API int file_ftruncate(int id, off_t size);
KERNEL_API int file_fsync(int id);
KERNEL_API int file_fdatasync(int id);
KERNEL_API int file_syncfs(int id);
KERNEL_API int file_ioctl(int id, int cmd, void* arg, unsigned flags, void *data);
KERNEL_API int file_lock_op(int id, flock &info, bool wait);
KERNEL_API int file_opendirat(int dirid, char const *path);
KERNEL_API ssize_t file_readdir_r(int id, dirent_t *buf, dirent_t **result);
KERNEL_API off_t file_telldir(int id);
KERNEL_API off_t file_seekdir(int id, off_t ofs);
KERNEL_API int file_closedir(int id);

KERNEL_API int file_mkdirat(int dirid, char const *path, mode_t mode);
KERNEL_API int file_rmdirat(int dirid, char const *path);
KERNEL_API int file_renameat(int olddirid, char const *old_path,
                  int newdirid, char const *new_path);
KERNEL_API int file_unlinkat(int dirid, char const *path);

KERNEL_API int file_fchmod(int id, mode_t mode);
KERNEL_API int file_chown(int id, int uid, int gid);

KERNEL_API int file_fstatfs(int id, fs_statvfs_t *buf);

__END_DECLS

#define AT_FDCWD -100

class file_t {
public:
    file_t()
        : fd(-1)
    {
    }

    explicit file_t(int fd)
        : fd(fd)
    {
    }

    file_t(file_t&& rhs) noexcept
        : fd(rhs.fd)
    {
        rhs.fd = -1;
    }

    file_t(file_t const&) = delete;
    file_t& operator=(file_t const&) = delete;

    file_t& operator=(file_t rhs)
    {
        if (fd != rhs.fd)
            close();
        fd = rhs.fd;
        rhs.fd = -1;
        return *this;
    }

    ~file_t()
    {
        close();
    }

    int release()
    {
        int result = fd;
        fd = -1;
        return result;
    }

    int close()
    {
        if (fd >= 0) {
            int result = file_close(fd);
            fd = -1;
            return result;
        }
        return 0;
    }

    file_t& operator=(int fd)
    {
        close();
        this->fd = fd;
        return *this;
    }

    operator int() const
    {
        return fd;
    }

    bool is_open() const
    {
        return fd >= 0;
    }

    operator bool() const
    {
        return is_open();
    }

private:
    int fd;
};

template<typename A = uintptr_t, typename S = size_t>
class basic_pmd_t
{
public:
    A addr;
    S size;

    constexpr A end() const { return addr + size; }
};

using pmd_t = basic_pmd_t<>;

// A representation for a sequence of address ranges
// Contiguous adds will be coalesced with the last entry
// Discontiguity causes a new pmd range to start
// The ranges need not be memory
template<typename A = uintptr_t, typename S = size_t>
class pmd_seq_t
{
public:
    using pmt_t = basic_pmd_t<A, S>;

    void add(A addr, S size)
    {
        if (!entries.empty()) {
            pmd_t& last = entries.back();
            if (addr == last.end()) {
                last.size += size;
                return;
            }
        }
        entries.push_back({addr, size});
    }

    size_t size()
    {
        return entries.size();
    }

    pmd_t& item(size_t index)
    {
        return entries.at(index);
    }

    pmd_t& operator[](size_t index)
    {
        return entries.at(index);
    }

    // Pass 16 to ensure no range crosses a 64KB boundary
    void apply_boundaries(S log2_boundary)
    {
        S boundary = S(1) << log2_boundary;

        for (size_t i = 0; i < entries.size(); ++i) {
            auto& entry = entries[i];
            auto st = entry.addr;
            auto max_end = (st + boundary) & -boundary;
            auto en = entry.end();
            if (en > max_end) {
                pmd_t replacement{st, max_end - st};
                pmd_t remainder{max_end, en - max_end};
                // This may invalidate all iterators and references
                entries.insert(entries.begin() + (i + 1), remainder);
                // Use [] because reference may be invalidated
                entries[i] = replacement;
            }
        }
    }

    void clamp_size(size_t log2_max_size)
    {
        S max_size = S(1) << log2_max_size;

        for (size_t i = 0; i < entries.size(); ++i) {
            auto& entry = entries[i];
            auto st = entry.addr;
            auto max_end = st + max_size;
            auto en = entry.end();
            if (entry.size > max_size) {
                pmd_t replacement{st, max_end - st};
                pmd_t remainder{max_end, en - max_end};
                // This may invalidate all iterators and references
                entries.insert(entries.begin() + (i + 1), remainder);
                // Use [] because reference may be invalidated
                entries[i] = replacement;
            }
        }
    }

private:
    ext::vector<pmd_t> entries;
};

// 64-bit
struct path_frag_t {
    // Half open range, start offset and offset of first character past end
    uint16_t st;
    uint16_t en;
    // Hash
    uint32_t hash;
};

// Safely bring a path string into a large object
struct path_t {
    // Locked down tight, create once, no copy, no assign
    path_t(char const *user_path);
    path_t(path_t const&) = delete;
    path_t operator=(path_t const&) = delete;

    operator bool() const;

    // Worst cases:
    // up to 4095 characters of path
    // up to 2048 path components

    size_t size() const;

    size_t text_len() const;

    uint32_t hash_of(size_t index) const;

    // Null terminated, so c_str() like behaviour as well
    char const *begin_of(size_t index) const;

    char const *end_of(size_t index) const;

    size_t len_of(size_t index) const;

    ext::pair<char const *, char const *> range_of(size_t index) const;

    bool is_abs() const;

    bool is_unc() const;

    bool is_relative() const;

    bool is_valid() const;

    errno_t error() const;

    char const *operator[](size_t component) const;

#ifdef NDEBUG
    __attribute__((__deprecated__("For debugging only")))
#endif
    ext::string to_string() const;

private:
    static const constexpr size_t path_buf_sz =
            sizeof(path_frag_t) * (PATH_MAX/2) + PATH_MAX;

    static_assert(path_buf_sz < 24 * 1024, "Will consume excessive stack");

    path_frag_t *components = nullptr;

    uint_fast16_t nr_components = 0;
    errno_t err = errno_t::OK;
    bool valid = false;
    bool absolute = false;
    bool unc = false;

    typename ext::aligned_storage<path_buf_sz, sizeof(char const*)>::type data;
};

int file_create_socket();
