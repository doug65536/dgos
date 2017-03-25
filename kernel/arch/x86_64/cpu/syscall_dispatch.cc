#include "syscall_dispatch.h"
#include "types.h"

typedef struct fd_t {

    off_t seek_pos;
} fd_t;

static long sys_read(
        long fd, long bufaddr, long count,
        long p3, long p4, long p5)
{
    (void)p5;
    (void)p4;
    (void)p3;

    (void)fd;
    (void)bufaddr;
    (void)count;

    return -1;
}

static long sys_write(
        long fd, long bufaddr, long count,
        long p3, long p4, long p5)
{
    (void)p5;
    (void)p4;
    (void)p3;

    (void)fd;
    (void)bufaddr;
    (void)count;

    return -1;
}

static long sys_open(
        long pathname, long flags, long mode,
        long p3, long p4, long p5)
{
    (void)p5;
    (void)p4;
    (void)p3;

    (void)pathname;
    (void)flags;
    (void)mode;

    return -1;
}

static long sys_close(
        long fd, long p1, long p2,
        long p3, long p4, long p5)
{
    (void)p5;
    (void)p4;
    (void)p3;
    (void)p2;
    (void)p1;

    (void)fd;

    return -1;
}

static long sys_lseek(
        long fd, long ofs, long rel,
        long p3, long p4, long p5)
{
    (void)p5;
    (void)p4;
    (void)p3;

    (void)fd;
    (void)ofs;
    (void)rel;

    return -1;
}

static long sys_pread64(
        long fd, long bufaddr, long count, long ofs,
        long p4, long p5)
{
    (void)p5;
    (void)p4;

    (void)fd;
    (void)bufaddr;
    (void)count;
    (void)ofs;

    return -1;
}

static long sys_pwrite64(
        long fd, long bufaddr, long count, long ofs,
        long p4, long p5)
{
    (void)p5;
    (void)p4;

    (void)fd;
    (void)bufaddr;
    (void)count;
    (void)ofs;

    return -1;
}

syscall_handler_t *syscall_handlers[314] = {
    sys_read,
    sys_write,
    sys_open,
    sys_close,
    0,//sys_newstat,
    0,//sys_newfstat,
    0,//sys_newlstat,
    0,//sys_poll,
    sys_lseek,
    0,//sys_mmap,
    0,//sys_mprotect,
    0,//sys_munmap,
    0,//sys_brk,
    0,//sys_rt_sigaction,
    0,//sys_rt_sigprocmask,
    0,//sys_stub_rt_sigreturn,
    0,//sys_sys_ioctl,
    sys_pread64,
    sys_pwrite64,
    0,//sys_readv,
    0,//sys_writev,
    0,//sys_access,
    0,//sys_pipe,
    0,//sys_select,
    0,//sys_sched_yield,
    0,//sys_mremap,
    0,//sys_msync,
    0,//sys_mincore,
    0,//sys_madvise,
    0,//sys_shmget,
    0,//sys_shmat,
    0,//sys_shmctl,
    0,//sys_dup,
    0,//sys_dup2,
    0,//sys_pause,
    0,//sys_nanosleep,
    0,//sys_getitimer,
    0,//sys_alarm,
    0,//sys_setitimer,
    0,//sys_getpid,
    0,//sys_sendfile,
    0,//sys_socket,
    0,//sys_connect,
    0,//sys_accept,
    0,//sys_sendto,
    0,//sys_recvfrom,
    0,//sys_sendmsg,
    0,//sys_recvmsg,
    0,//sys_shutdown,
    0,//sys_bind,
    0,//sys_listen,
    0,//sys_getsockname,
    0,//sys_getpeername,
    0,//sys_socketpair,
    0,//sys_setsockopt,
    0,//sys_getsockopt,
    0,//sys_clone,
    0,//sys_fork,
    0,//sys_vfork,
    0,//sys_execve,
    0,//sys_exit,
    0,//sys_wait4,
    0,//sys_kill,
    0,//sys_uname,
    0,//sys_semget,
    0,//sys_semop,
    0,//sys_semctl,
    0,//sys_shmdt,
    0,//sys_msgget,
    0,//sys_msgsnd,
    0,//sys_msgrcv,
    0,//sys_msgctl,
    0,//sys_fcntl,
    0,//sys_flock,
    0,//sys_fsync,
    0,//sys_fdatasync,
    0,//sys_truncate,
    0,//sys_ftruncate,
    0,//sys_getdents,
    0,//sys_getcwd,
    0,//sys_chdir,
    0,//sys_fchdir,
    0,//sys_rename,
    0,//sys_mkdir,
    0,//sys_rmdir,
    0,//sys_creat,
    0,//sys_link,
    0,//sys_unlink,
    0,//sys_symlink,
    0,//sys_readlink,
    0,//sys_chmod,
    0,//sys_fchmod,
    0,//sys_chown,
    0,//sys_fchown,
    0,//sys_lchown,
    0,//sys_umask,
    0,//sys_gettimeofday,
    0,//sys_getrlimit,
    0,//sys_getrusage,
    0,//sys_sysinfo,
    0,//sys_times,
    0,//sys_ptrace,
    0,//sys_getuid,
    0,//sys_syslog,
    0,//sys_getgid,
    0,//sys_setuid,
    0,//sys_setgid,
    0,//sys_geteuid,
    0,//sys_getegid,
    0,//sys_setpgid,
    0,//sys_getppid,
    0,//sys_getpgrp,
    0,//sys_setsid,
    0,//sys_setreuid,
    0,//sys_setregid,
    0,//sys_getgroups,
    0,//sys_setgroups,
    0,//sys_setresuid,
    0,//sys_getresuid,
    0,//sys_setresgid,
    0,//sys_getresgid,
    0,//sys_getpgid,
    0,//sys_setfsuid,
    0,//sys_setfsgid,
    0,//sys_getsid,
    0,//sys_capget,
    0,//sys_capset,
    0,//sys_rt_sigpending,
    0,//sys_rt_sigtimedwait,
    0,//sys_rt_sigqueueinfo,
    0,//sys_rt_sigsuspend,
    0,//sys_sigaltstack,
    0,//sys_utime,
    0,//sys_mknod,
    0,//sys_uselib,
    0,//sys_personality,
    0,//sys_ustat,
    0,//sys_statfs,
    0,//sys_fstatfs,
    0,//sys_sysfs,
    0,//sys_getpriority,
    0,//sys_setpriority,
    0,//sys_sched_setparam,
    0,//sys_sched_getparam,
    0,//sys_sched_setscheduler,
    0,//sys_sched_getscheduler,
    0,//sys_sched_get_priority_max,
    0,//sys_sched_get_priority_min,
    0,//sys_sched_rr_get_interval,
    0,//sys_mlock,
    0,//sys_munlock,
    0,//sys_mlockall,
    0,//sys_munlockall,
    0,//sys_vhangup,
    0,//sys_modify_ldt,
    0,//sys_pivot_root,
    0,//sys__sysctl,
    0,//sys_prctl,
    0,//sys_arch_prctl,
    0,//sys_adjtimex,
    0,//sys_setrlimit,
    0,//sys_chroot,
    0,//sys_sync,
    0,//sys_acct,
    0,//sys_settimeofday,
    0,//sys_mount,
    0,//sys_umount2,
    0,//sys_swapon,
    0,//sys_swapoff,
    0,//sys_reboot,
    0,//sys_sethostname,
    0,//sys_setdomainname,
    0,//sys_iopl,
    0,//sys_ioperm,
    0,//sys_create_module,
    0,//sys_init_module,
    0,//sys_delete_module,
    0,//sys_get_kernel_syms,
    0,//sys_query_module,
    0,//sys_quotactl,
    0,//sys_nfsservctl,
    0,//sys_getpmsg,
    0,//sys_putpmsg,
    0,//sys_afs_syscall,
    0,//sys_tuxcall,
    0,//sys_security,
    0,//sys_gettid,
    0,//sys_readahead,
    0,//sys_setxattr,
    0,//sys_lsetxattr,
    0,//sys_fsetxattr,
    0,//sys_getxattr,
    0,//sys_lgetxattr,
    0,//sys_fgetxattr,
    0,//sys_listxattr,
    0,//sys_llistxattr,
    0,//sys_flistxattr,
    0,//sys_removexattr,
    0,//sys_lremovexattr,
    0,//sys_fremovexattr,
    0,//sys_tkill,
    0,//sys_time,
    0,//sys_futex,
    0,//sys_sched_setaffinity,
    0,//sys_sched_getaffinity,
    0,//sys_set_thread_area,
    0,//sys_io_setup,
    0,//sys_io_destroy,
    0,//sys_io_getevents,
    0,//sys_io_submit,
    0,//sys_io_cancel,
    0,//sys_get_thread_area,
    0,//sys_lookup_dcookie,
    0,//sys_epoll_create,
    0,//sys_epoll_ctl_old,
    0,//sys_epoll_wait_old,
    0,//sys_remap_file_pages,
    0,//sys_getdents64,
    0,//sys_set_tid_address,
    0,//sys_restart_syscall,
    0,//sys_semtimedop,
    0,//sys_fadvise64,
    0,//sys_timer_create,
    0,//sys_timer_settime,
    0,//sys_timer_gettime,
    0,//sys_timer_getoverrun,
    0,//sys_timer_delete,
    0,//sys_clock_settime,
    0,//sys_clock_gettime,
    0,//sys_clock_getres,
    0,//sys_clock_nanosleep,
    0,//sys_exit_group,
    0,//sys_epoll_wait,
    0,//sys_epoll_ctl,
    0,//sys_tgkill,
    0,//sys_utimes,
    0,//sys_vserver,
    0,//sys_mbind,
    0,//sys_set_mempolicy,
    0,//sys_get_mempolicy,
    0,//sys_mq_open,
    0,//sys_mq_unlink,
    0,//sys_mq_timedsend,
    0,//sys_mq_timedreceive,
    0,//sys_mq_notify,
    0,//sys_mq_getsetattr,
    0,//sys_kexec_load,
    0,//sys_waitid,
    0,//sys_add_key,
    0,//sys_request_key,
    0,//sys_keyctl,
    0,//sys_ioprio_set,
    0,//sys_ioprio_get,
    0,//sys_inotify_init,
    0,//sys_inotify_add_watch,
    0,//sys_inotify_rm_watch,
    0,//sys_migrate_pages,
    0,//sys_openat,
    0,//sys_mkdirat,
    0,//sys_mknodat,
    0,//sys_fchownat,
    0,//sys_futimesat,
    0,//sys_newfstatat,
    0,//sys_unlinkat,
    0,//sys_renameat,
    0,//sys_linkat,
    0,//sys_symlinkat,
    0,//sys_readlinkat,
    0,//sys_fchmodat,
    0,//sys_faccessat,
    0,//sys_pselect6,
    0,//sys_ppoll,
    0,//sys_unshare,
    0,//sys_set_robust_list,
    0,//sys_get_robust_list,
    0,//sys_splice,
    0,//sys_tee,
    0,//sys_sync_file_range,
    0,//sys_vmsplice,
    0,//sys_move_pages,
    0,//sys_utimensat,
    0,//sys_epoll_pwait,
    0,//sys_signalfd,
    0,//sys_timerfd_create,
    0,//sys_eventfd,
    0,//sys_fallocate,
    0,//sys_timerfd_settime,
    0,//sys_timerfd_gettime,
    0,//sys_accept4,
    0,//sys_signalfd4,
    0,//sys_eventfd2,
    0,//sys_epoll_create1,
    0,//sys_dup3,
    0,//sys_pipe2,
    0,//sys_inotify_init1,
    0,//sys_preadv,
    0,//sys_pwritev,
    0,//sys_rt_tgsigqueueinfo,
    0,//sys_perf_event_open,
    0,//sys_recvmmsg,
    0,//sys_fanotify_init,
    0,//sys_fanotify_mark,
    0,//sys_prlimit64,
    0,//sys_name_to_handle_at,
    0,//sys_open_by_handle_at,
    0,//sys_clock_adjtime,
    0,//sys_syncfs,
    0,//sys_sendmmsg,
    0,//sys_setns,
    0,//sys_getcpu,
    0,//sys_process_vm_readv,
    0,//sys_process_vm_writev,
    0,//sys_kcmp,
    0//sys_finit_module
};
