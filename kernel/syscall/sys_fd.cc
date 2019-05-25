#include "sys_fd.h"
#include "process.h"
#include "thread.h"
#include "fileio.h"
#include "syscall_helper.h"
#include "../libc/include/sys/ioctl.h"
#include "mm.h"
#include "unique_ptr.h"

class unique_memlock
{
public:
    unique_memlock()
        : locked_addr{}
        , locked_size{}
    {
    }

    unique_memlock(std::defer_lock_t)
        : unique_memlock()
    {
    }

    unique_memlock(unique_memlock&& rhs)
        : locked_addr{rhs.locked_addr}
        , locked_size{rhs.locked_size}
    {
        rhs.locked_addr = nullptr;
        rhs.locked_size = 0;
    }

    explicit unique_memlock(void const *addr, size_t size)
        : unique_memlock()
    {
        lock(addr, size);
    }

    ~unique_memlock()
    {
        unlock();
    }

    int lock(void const *addr, size_t size)
    {
        unlock();
        if (mlock(addr, size) < 0)
            return -1;
        locked_addr = addr;
        locked_size = size;
        return 0;
    }

    void unlock()
    {
        if (locked_size) {
            munlock(locked_addr, locked_size);
            locked_addr = nullptr;
            locked_size = 0;
        }
    }

private:
    void const *locked_addr;
    size_t locked_size;
};

static int err(errno_t errno);
static int arg_err();
static int fault_err();
static int nosys_err();
static int toomany_err();
static int badf_err();
static int err(int negerr);

// == APIs that take file descriptors ==

static int id_from_fd(int fd)
{
    process_t *p = fast_cur_process();

    if (unlikely(fd < 0))
        return badf_err();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    return id;
}

ssize_t sys_read(int fd, void *bufaddr, size_t count)
{
    if (unlikely(!mm_is_user_range(bufaddr, count)))
        return fault_err();

    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    unique_memlock memlock(bufaddr, count);

    ssize_t sz = file_read(id, bufaddr, count);

    if (likely(sz >= 0))
        return sz;

    return err(sz);
}

ssize_t sys_write(int fd, void const *bufaddr, size_t count)
{
    if (unlikely(!mm_is_user_range(bufaddr, count)))
        return fault_err();

    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    if (uintptr_t(bufaddr) >= 0x800000000000)
        return fault_err();

    unique_memlock memlock(bufaddr, count);

    if (!verify_accessible(bufaddr, count, false))
        return fault_err();

    ssize_t sz = file_write(id, bufaddr, count);

    if (likely(sz >= 0))
        return sz;

    return err(sz);
}

int sys_close(int fd)
{
    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_close(id);
    if (likely(status == 0))
        return 0;

    return err(status);
}

ssize_t sys_pread64(int fd, void *bufaddr, size_t count, off_t ofs)
{
    if (unlikely(!mm_is_user_range(bufaddr, count)))
        return fault_err();

    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    int sz = file_pread(id, bufaddr, count, ofs);
    if (likely(sz >= 0))
        return sz;

    return err(sz);
}

ssize_t sys_pwrite64(int fd, void const *bufaddr,
                     size_t count, off_t ofs)
{
    if (unlikely(!mm_is_user_range(bufaddr, count)))
        return fault_err();

    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    int sz = file_pwrite(id, bufaddr, count, ofs);
    if (likely(sz >= 0))
        return sz;

    return err(sz);
}

off_t sys_lseek(int fd, off_t ofs, int whence)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(fd);

    if (unlikely(id < 0))
        return badf_err();

    off_t pos = file_seek(id, ofs, whence);
    if (likely(pos >= 0))
        return pos;

    return err(pos);
}

int sys_fsync(int fd)
{
    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_fsync(id);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_fdatasync(int fd)
{
    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_fdatasync(id);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_ftruncate(int fd, off_t size)
{
    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    int status = file_ftruncate(id, size);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_ioctl(int fd, int cmd, void* arg)
{
    size_t size = _IOC_SIZE(cmd);

    // If the size is nonzero and the argument pointer does not lie in user
    // address range, then fail with EINVAL
    if (unlikely(size > 0 && !mm_is_user_range(arg, size)))
        return arg_err();

    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    ext::unique_ptr_free<void> data;

    // If the command indicates read or write, and the size is nonzero,
    // then allocate a kernel shadow buffer of the argument pointer data
    if ((_IOC_DIR(cmd) & _IOC_RDWR) && size > 0) {
        if (unlikely(!data.reset(calloc(1, size))))
            return err(errno_t::ENOMEM);
    }

    // Copy the argument to a kernel memory buffer if WRITE
    if ((_IOC_DIR(cmd) & _IOC_WRITE) && size > 0) {
        if (!mm_copy_user(data, arg, size))
            return fault_err();
    }

    int status = file_ioctl(id, cmd, arg, size, data);

    // Copy the argument to user buffer if READ
    if ((_IOC_DIR(cmd) & _IOC_READ) && size > 0) {
        if (!mm_copy_user(arg, data, size))
            return fault_err();
    }

    return status;
}

int sys_dup(int oldfd)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(oldfd);

    int newfd = p->ids.desc_alloc.alloc();

    if (likely(file_ref_filetab(id))) {
        p->ids.ids[newfd] = id;
        return newfd;
    }

    p->ids.desc_alloc.free(newfd);

    return badf_err();
}

int sys_dup3(int oldfd, int newfd, int flags)
{
    process_t *p = fast_cur_process();

    int id = p->fd_to_id(oldfd);

    int newid = p->fd_to_id(newfd);

    if (newid >= 0)
        file_close(newid);
    else if (!p->ids.desc_alloc.take(newfd))
        return toomany_err();


    if (likely(file_ref_filetab(id))) {
        p->ids.ids[newfd].set(id, flags);
        return newfd;
    }

    p->ids.desc_alloc.free(newfd);

    return badf_err();
}

int sys_dup2(int oldfd, int newfd)
{
    return sys_dup3(oldfd, newfd, 0);
}

// == APIs that take paths ==

int sys_open(char const* pathname, int flags, mode_t mode)
{
    user_str_t path(pathname);

    if (unlikely(!path))
        return path.err_int();

    process_t *p = fast_cur_process();

    int fd = p->ids.desc_alloc.alloc();

    int id = file_open(path, flags, mode);

    if (likely(id >= 0)) {
        p->ids.ids[fd] = id;
        return fd;
    }

    p->ids.desc_alloc.free(fd);
    return err(-id);
}

int sys_creat(char const *pathname, mode_t mode)
{
    user_str_t path(pathname);

    process_t *p = fast_cur_process();

    int fd = p->ids.desc_alloc.alloc();

    if (unlikely(fd < 0))
        return toomany_err();

    int id = file_creat(path, mode);

    if (likely(id >= 0)) {
        p->ids.ids[fd] = id;
        return fd;
    }

    p->ids.desc_alloc.free(fd);
    return err(-id);
}

int sys_truncate(char const *path, off_t size)
{
    int fd = sys_open(path, O_RDWR, 0);
    int err = sys_ftruncate(fd, size);
    sys_close(fd);
    return err;
}

int sys_rename(char const *old_pathname, char const *new_pathname)
{
    std::unique_ptr<user_str_t> old_path_storage(new user_str_t(old_pathname));

    if (unlikely(!old_path_storage))
        return old_path_storage->err_int();

    std::unique_ptr<user_str_t> new_path_storage(new user_str_t(new_pathname));

    if (unlikely(!new_path_storage))
        return new_path_storage->err_int();

    char const *old_path = *old_path_storage;
    char const *new_path = *new_path_storage;

    if (unlikely(!old_path))
        return fault_err();

    if (unlikely(!new_path))
        return fault_err();

    int status = file_rename(old_path, new_path);
    if (unlikely(status < 0))
        return err(status);

    return status;
}

int sys_mkdir(char const *path, mode_t mode)
{
    int status = file_mkdir(path, mode);
    if (unlikely(status < 0))
        return err(status);

    return status;
}

int sys_rmdir(char const *path)
{
    int status = file_rmdir(path);
    if (unlikely(status < 0))
        return err(status);

    return status;
}

int sys_unlink(char const *path)
{
    int status = file_unlink(path);
    if (likely(status >= 0))
        return status;

    return err(status);
}

int sys_mknod(char const *path, mode_t mode, int rdev)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_link(char const *from, char const *to)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_chmod(char const *path, mode_t mode)
{
    file_t id(file_open(path, O_EXCL));
    if (unlikely(id < 0))
        return id;

    int chmod_status = sys_fchmod(id, mode);

    int close_status = id.close();

    // If the actual chmod failed, and the close failed, report the chmod
    // failure and drop the probably impossible close error

    if (unlikely(close_status < 0 && chmod_status == 0))
        return close_status;

    return chmod_status;
}

int sys_fchmod(int fd, mode_t mode)
{
    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    return file_fchmod(id, mode);
}

int sys_chown(char const *path, int uid, int gid)
{
    file_t id(file_open(path, O_EXCL));
    if (unlikely(id < 0))
        return id;

    int chmod_status = sys_fchown(id, uid, gid);

    int close_status = id.close();

    // If the actual chmod failed, and the close failed, report the chmod
    // failure and drop the probably impossible close error

    if (unlikely(close_status < 0 && chmod_status == 0))
        return close_status;

    return chmod_status;
}

int sys_chownat(int dirfd, char const *path, int uid, int gid)
{
    return nosys_err();
}

int sys_fchown(int fd, int uid, int gid)
{
    int id = id_from_fd(fd);

    if (unlikely(id < 0))
        return badf_err();

    int chown_status = file_chown(id, uid, gid);

    return chown_status;
}

int sys_setxattr(char const *path,
                 char const *name, char const *value,
                 size_t size, int flags)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_getxattr(char const *path,
                 char const *name, char *value,
                 size_t size)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_listxattr(char const *path, char const *list, size_t size)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_access(char const *path, int mask)
{
    // FIXME: implement me
    return nosys_err();
}

// == Sockets ==

int sys_socket(int domain, int type, int protocol)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // FIXME: implement me
    return nosys_err();
}

int sys_accept(int sockfd, sockaddr *addr, socklen_t *addrlen)
{
    // FIXME: implement me
    return nosys_err();
}

ssize_t sys_send(int sockfd, void const *buf, size_t len, int flags)
{
    return nosys_err();
}

ssize_t sys_sendto(int sockfd, void const *buf, size_t len, int flags,
               struct sockaddr const *dest_addr, socklen_t addrlen)
{
    return nosys_err();
}

ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags)
{
    return nosys_err();
}

ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen)
{
    return nosys_err();
}

int sys_shutdown(int sockfd, int how)
{
    return nosys_err();
}

int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return nosys_err();
}

int sys_listen(int sockfd, int backlog)
{
    return nosys_err();
}

int sys_fcntl(int fd, int cmd, void *arg)
{
    //switch (cmd) {
//    case F_DUPFD:
//    case F_DUPFD_CLOEXEC:
//    case F_GETFD:
//    case F_SETFD:
//    case F_GETFL:
//    case F_SETFL:
//    case F_SETLK:
//    case F_SETLKW:
//    case F_GETLK:
//    }
    return nosys_err();
}

char *sys_getcwd(char *buf, size_t size)
{
    return (char*)-intptr_t(nosys_err());
}

// Validate the errno and return its negated integer value
static int err(errno_t errno)
{
    assert(int(errno) > int(errno_t::OK) &&
           int(errno) < int(errno_t::MAX_ERRNO));
    return -int(errno);
}

// Bottom to make unlikely path already naturally be a forward branch

static int err(int negerr)
{
    assert(negerr < 0 && -negerr < int(errno_t::MAX_ERRNO));
    return negerr;
}

static int badf_err()
{
    return err(errno_t::EBADF);
}

static int toomany_err()
{
    return err(errno_t::EMFILE);
}

static int fault_err()
{
    return err(errno_t::EFAULT);
}

static int arg_err()
{
    return err(errno_t::EINVAL);
}

static int nosys_err()
{
    return err(errno_t::ENOSYS);
}