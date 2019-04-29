#pragma once
#include "types.h"
#include "dirent.h"

#include "vector.h"
#include "cxxstring.h"
#include "syscall/sys_limits.h"

#define SEEK_CUR    0
#define SEEK_SET    1
#define SEEK_END    2
#define SEEK_DATA   3
#define SEEK_HOLE   4

struct path_t;

bool file_ref_filetab(int id);

int file_creat(char const *path, mode_t mode);
int file_open(char const* path, int flags, mode_t mode = 0);
int file_close(int id);
ssize_t file_read(int id, void *buf, size_t bytes);
ssize_t file_write(int id, void const *buf, size_t bytes);
ssize_t file_pread(int id, void *buf, size_t bytes, off_t ofs);
ssize_t file_pwrite(int id, void const *buf, size_t bytes, off_t ofs);
off_t file_seek(int id, off_t ofs, int whence);
int file_ftruncate(int id, off_t size);
int file_fsync(int id);
int file_fdatasync(int id);
int file_syncfs(int id);
int file_ioctl(int id, int cmd, void* arg, unsigned flags, void *data);

int file_opendir(char const *path);
ssize_t file_readdir_r(int id, dirent_t *buf, dirent_t **result);
off_t file_telldir(int id);
off_t file_seekdir(int id, off_t ofs);
int file_closedir(int id);

int file_mkdir(char const *path, mode_t mode);
int file_rmdir(char const *path);
int file_rename(char const *old_path, char const *new_path);
int file_unlink(char const *path);

int file_fchmod(int id, mode_t mode);
int file_chown(int id, int uid, int gid);

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

    file_t(file_t&& rhs)
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
    std::vector<pmd_t> entries;
};

// 64-bit
struct path_frag_t {
    // Half open range, start offset and offset of first character past end
    uint16_t st;
    uint16_t en;
    // Hash
    uint32_t hash;
};

struct user_str_t {
    user_str_t(char const *user_str);
    user_str_t(user_str_t const&) = delete;
    user_str_t operator=(user_str_t const&) = delete;

    // Returns true if holding a valid null terminated string
    operator bool() const
    {
        return lenof_str >= 0;
    }

    // Return a pointer to the kernel copy of the string if there is
    // a string present, otherwise returns nullptr
    operator char const *() const
    {
        return lenof_str >= 0
                ? reinterpret_cast<char const *>(&data)
                : nullptr;
    }

    // Returns the length of the string, excluding the null terminator
    // if there is a string present, otherwise returns 0
    size_t len() const
    {
        return lenof_str >= 0 ? lenof_str : 0;
    }

    // Returns the errno of the user-to-kernel transfer result
    // Successful use returns errno_t::OK
    errno_t err() const
    {
        return lenof_str < 0 ? errno_t(-lenof_str) : errno_t::OK;
    }

    // Returns the negated integer errno of the user-to-kernel transfer result
    // Successful use returns errno_t::OK
    int err_int() const
    {
        return lenof_str < 0 ? lenof_str : int(errno_t::OK);
    }

    void set_err(errno_t err)
    {
        lenof_str = -intptr_t(err);
    }

    static constexpr size_t max_sz = PATH_MAX;

    // Doubles as length storage when >= 0
    // otherwise holds negated errno value
    intptr_t lenof_str;
    typename std::aligned_storage<max_sz, sizeof(char const*)>::type data;
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

    std::pair<char const *, char const *> range_of(size_t index) const;

    bool is_abs() const;

    bool is_unc() const;

    bool is_relative() const;

    bool is_valid() const;

    errno_t error() const;

    char const *operator[](size_t component) const;

#ifdef NDEBUG
    __attribute__((__deprecated__("For debugging only")))
#endif
    std::string to_string() const;

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

    typename std::aligned_storage<path_buf_sz, sizeof(char const*)>::type data;
};
