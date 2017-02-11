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
    [  0] = sys_read,
    [  1] = sys_write,
    [  2] = sys_open,
    [  3] = sys_close,
    [  4] = 0,//sys_newstat,
    [  5] = 0,//sys_newfstat,
    [  6] = 0,//sys_newlstat,
    [  7] = 0,//sys_poll,
    [  8] = sys_lseek,
    [  9] = 0,//sys_mmap,
    [ 10] = 0,//sys_mprotect,
    [ 11] = 0,//sys_munmap,
    [ 12] = 0,//sys_brk,
    [ 13] = 0,//sys_rt_sigaction,
    [ 14] = 0,//sys_rt_sigprocmask,
    [ 15] = 0,//sys_stub_rt_sigreturn,
    [ 16] = 0,//sys_sys_ioctl,
    [ 17] = sys_pread64,
    [ 18] = sys_pwrite64,
    [ 19] = 0,//sys_readv,
    [ 20] = 0,//sys_writev,
    [ 21] = 0,//sys_access,
    [ 22] = 0,//sys_pipe,
    [ 23] = 0,//sys_select,
    [ 24] = 0,//sys_sched_yield,
    [ 25] = 0,//sys_mremap,
    [ 26] = 0,//sys_msync,
    [ 27] = 0,//sys_mincore,
    [ 28] = 0,//sys_madvise,
    [ 29] = 0,//sys_shmget,
    [ 30] = 0,//sys_shmat,
    [ 31] = 0,//sys_shmctl,
    [ 32] = 0,//sys_dup,
    [ 33] = 0,//sys_dup2,
    [ 34] = 0,//sys_pause,
    [ 35] = 0,//sys_nanosleep,
    [ 36] = 0,//sys_getitimer,
    [ 37] = 0,//sys_alarm,
    [ 38] = 0,//sys_setitimer,
    [ 39] = 0,//sys_getpid,
    [ 40] = 0,//sys_sendfile,
    [ 41] = 0,//sys_socket,
    [ 42] = 0,//sys_connect,
    [ 43] = 0,//sys_accept,
    [ 44] = 0,//sys_sendto,
    [ 45] = 0,//sys_recvfrom,
    [ 46] = 0,//sys_sendmsg,
    [ 47] = 0,//sys_recvmsg,
    [ 48] = 0,//sys_shutdown,
    [ 49] = 0,//sys_bind,
    [ 50] = 0,//sys_listen,
    [ 51] = 0,//sys_getsockname,
    [ 52] = 0,//sys_getpeername,
    [ 53] = 0,//sys_socketpair,
    [ 54] = 0,//sys_setsockopt,
    [ 55] = 0,//sys_getsockopt,
    [ 56] = 0,//sys_clone,
    [ 57] = 0,//sys_fork,
    [ 58] = 0,//sys_vfork,
    [ 59] = 0,//sys_execve,
    [ 60] = 0,//sys_exit,
    [ 61] = 0,//sys_wait4,
    [ 62] = 0,//sys_kill,
    [ 63] = 0,//sys_uname,
    [ 64] = 0,//sys_semget,
    [ 65] = 0,//sys_semop,
    [ 66] = 0,//sys_semctl,
    [ 67] = 0,//sys_shmdt,
    [ 68] = 0,//sys_msgget,
    [ 69] = 0,//sys_msgsnd,
    [ 70] = 0,//sys_msgrcv,
    [ 71] = 0,//sys_msgctl,
    [ 72] = 0,//sys_fcntl,
    [ 73] = 0,//sys_flock,
    [ 74] = 0,//sys_fsync,
    [ 75] = 0,//sys_fdatasync,
    [ 76] = 0,//sys_truncate,
    [ 77] = 0,//sys_ftruncate,
    [ 78] = 0,//sys_getdents,
    [ 79] = 0,//sys_getcwd,
    [ 80] = 0,//sys_chdir,
    [ 81] = 0,//sys_fchdir,
    [ 82] = 0,//sys_rename,
    [ 83] = 0,//sys_mkdir,
    [ 84] = 0,//sys_rmdir,
    [ 85] = 0,//sys_creat,
    [ 86] = 0,//sys_link,
    [ 87] = 0,//sys_unlink,
    [ 88] = 0,//sys_symlink,
    [ 89] = 0,//sys_readlink,
    [ 90] = 0,//sys_chmod,
    [ 91] = 0,//sys_fchmod,
    [ 92] = 0,//sys_chown,
    [ 93] = 0,//sys_fchown,
    [ 94] = 0,//sys_lchown,
    [ 95] = 0,//sys_umask,
    [ 96] = 0,//sys_gettimeofday,
    [ 97] = 0,//sys_getrlimit,
    [ 98] = 0,//sys_getrusage,
    [ 99] = 0,//sys_sysinfo,
    [100] = 0,//sys_times,
    [101] = 0,//sys_ptrace,
    [102] = 0,//sys_getuid,
    [103] = 0,//sys_syslog,
    [104] = 0,//sys_getgid,
    [105] = 0,//sys_setuid,
    [106] = 0,//sys_setgid,
    [107] = 0,//sys_geteuid,
    [108] = 0,//sys_getegid,
    [109] = 0,//sys_setpgid,
    [110] = 0,//sys_getppid,
    [111] = 0,//sys_getpgrp,
    [112] = 0,//sys_setsid,
    [113] = 0,//sys_setreuid,
    [114] = 0,//sys_setregid,
    [115] = 0,//sys_getgroups,
    [116] = 0,//sys_setgroups,
    [117] = 0,//sys_setresuid,
    [118] = 0,//sys_getresuid,
    [119] = 0,//sys_setresgid,
    [120] = 0,//sys_getresgid,
    [121] = 0,//sys_getpgid,
    [122] = 0,//sys_setfsuid,
    [123] = 0,//sys_setfsgid,
    [124] = 0,//sys_getsid,
    [125] = 0,//sys_capget,
    [126] = 0,//sys_capset,
    [127] = 0,//sys_rt_sigpending,
    [128] = 0,//sys_rt_sigtimedwait,
    [129] = 0,//sys_rt_sigqueueinfo,
    [130] = 0,//sys_rt_sigsuspend,
    [131] = 0,//sys_sigaltstack,
    [132] = 0,//sys_utime,
    [133] = 0,//sys_mknod,
    [134] = 0,//sys_uselib,
    [135] = 0,//sys_personality,
    [136] = 0,//sys_ustat,
    [137] = 0,//sys_statfs,
    [138] = 0,//sys_fstatfs,
    [139] = 0,//sys_sysfs,
    [140] = 0,//sys_getpriority,
    [141] = 0,//sys_setpriority,
    [142] = 0,//sys_sched_setparam,
    [143] = 0,//sys_sched_getparam,
    [144] = 0,//sys_sched_setscheduler,
    [145] = 0,//sys_sched_getscheduler,
    [146] = 0,//sys_sched_get_priority_max,
    [147] = 0,//sys_sched_get_priority_min,
    [148] = 0,//sys_sched_rr_get_interval,
    [149] = 0,//sys_mlock,
    [150] = 0,//sys_munlock,
    [151] = 0,//sys_mlockall,
    [152] = 0,//sys_munlockall,
    [153] = 0,//sys_vhangup,
    [154] = 0,//sys_modify_ldt,
    [155] = 0,//sys_pivot_root,
    [156] = 0,//sys__sysctl,
    [157] = 0,//sys_prctl,
    [158] = 0,//sys_arch_prctl,
    [159] = 0,//sys_adjtimex,
    [160] = 0,//sys_setrlimit,
    [161] = 0,//sys_chroot,
    [162] = 0,//sys_sync,
    [163] = 0,//sys_acct,
    [164] = 0,//sys_settimeofday,
    [165] = 0,//sys_mount,
    [166] = 0,//sys_umount2,
    [167] = 0,//sys_swapon,
    [168] = 0,//sys_swapoff,
    [169] = 0,//sys_reboot,
    [170] = 0,//sys_sethostname,
    [171] = 0,//sys_setdomainname,
    [172] = 0,//sys_iopl,
    [173] = 0,//sys_ioperm,
    [174] = 0,//sys_create_module,
    [175] = 0,//sys_init_module,
    [176] = 0,//sys_delete_module,
    [177] = 0,//sys_get_kernel_syms,
    [178] = 0,//sys_query_module,
    [179] = 0,//sys_quotactl,
    [180] = 0,//sys_nfsservctl,
    [181] = 0,//sys_getpmsg,
    [182] = 0,//sys_putpmsg,
    [183] = 0,//sys_afs_syscall,
    [184] = 0,//sys_tuxcall,
    [185] = 0,//sys_security,
    [186] = 0,//sys_gettid,
    [187] = 0,//sys_readahead,
    [188] = 0,//sys_setxattr,
    [189] = 0,//sys_lsetxattr,
    [190] = 0,//sys_fsetxattr,
    [191] = 0,//sys_getxattr,
    [192] = 0,//sys_lgetxattr,
    [193] = 0,//sys_fgetxattr,
    [194] = 0,//sys_listxattr,
    [195] = 0,//sys_llistxattr,
    [196] = 0,//sys_flistxattr,
    [197] = 0,//sys_removexattr,
    [198] = 0,//sys_lremovexattr,
    [199] = 0,//sys_fremovexattr,
    [200] = 0,//sys_tkill,
    [201] = 0,//sys_time,
    [202] = 0,//sys_futex,
    [203] = 0,//sys_sched_setaffinity,
    [204] = 0,//sys_sched_getaffinity,
    [205] = 0,//sys_set_thread_area,
    [206] = 0,//sys_io_setup,
    [207] = 0,//sys_io_destroy,
    [208] = 0,//sys_io_getevents,
    [209] = 0,//sys_io_submit,
    [210] = 0,//sys_io_cancel,
    [211] = 0,//sys_get_thread_area,
    [212] = 0,//sys_lookup_dcookie,
    [213] = 0,//sys_epoll_create,
    [214] = 0,//sys_epoll_ctl_old,
    [215] = 0,//sys_epoll_wait_old,
    [216] = 0,//sys_remap_file_pages,
    [217] = 0,//sys_getdents64,
    [218] = 0,//sys_set_tid_address,
    [219] = 0,//sys_restart_syscall,
    [220] = 0,//sys_semtimedop,
    [221] = 0,//sys_fadvise64,
    [222] = 0,//sys_timer_create,
    [223] = 0,//sys_timer_settime,
    [224] = 0,//sys_timer_gettime,
    [225] = 0,//sys_timer_getoverrun,
    [226] = 0,//sys_timer_delete,
    [227] = 0,//sys_clock_settime,
    [228] = 0,//sys_clock_gettime,
    [229] = 0,//sys_clock_getres,
    [230] = 0,//sys_clock_nanosleep,
    [231] = 0,//sys_exit_group,
    [232] = 0,//sys_epoll_wait,
    [233] = 0,//sys_epoll_ctl,
    [234] = 0,//sys_tgkill,
    [235] = 0,//sys_utimes,
    [236] = 0,//sys_vserver,
    [237] = 0,//sys_mbind,
    [238] = 0,//sys_set_mempolicy,
    [239] = 0,//sys_get_mempolicy,
    [240] = 0,//sys_mq_open,
    [241] = 0,//sys_mq_unlink,
    [242] = 0,//sys_mq_timedsend,
    [243] = 0,//sys_mq_timedreceive,
    [244] = 0,//sys_mq_notify,
    [245] = 0,//sys_mq_getsetattr,
    [246] = 0,//sys_kexec_load,
    [247] = 0,//sys_waitid,
    [248] = 0,//sys_add_key,
    [249] = 0,//sys_request_key,
    [250] = 0,//sys_keyctl,
    [251] = 0,//sys_ioprio_set,
    [252] = 0,//sys_ioprio_get,
    [253] = 0,//sys_inotify_init,
    [254] = 0,//sys_inotify_add_watch,
    [255] = 0,//sys_inotify_rm_watch,
    [256] = 0,//sys_migrate_pages,
    [257] = 0,//sys_openat,
    [258] = 0,//sys_mkdirat,
    [259] = 0,//sys_mknodat,
    [260] = 0,//sys_fchownat,
    [261] = 0,//sys_futimesat,
    [262] = 0,//sys_newfstatat,
    [263] = 0,//sys_unlinkat,
    [264] = 0,//sys_renameat,
    [265] = 0,//sys_linkat,
    [266] = 0,//sys_symlinkat,
    [267] = 0,//sys_readlinkat,
    [268] = 0,//sys_fchmodat,
    [269] = 0,//sys_faccessat,
    [270] = 0,//sys_pselect6,
    [271] = 0,//sys_ppoll,
    [272] = 0,//sys_unshare,
    [273] = 0,//sys_set_robust_list,
    [274] = 0,//sys_get_robust_list,
    [275] = 0,//sys_splice,
    [276] = 0,//sys_tee,
    [277] = 0,//sys_sync_file_range,
    [278] = 0,//sys_vmsplice,
    [279] = 0,//sys_move_pages,
    [280] = 0,//sys_utimensat,
    [281] = 0,//sys_epoll_pwait,
    [282] = 0,//sys_signalfd,
    [283] = 0,//sys_timerfd_create,
    [284] = 0,//sys_eventfd,
    [285] = 0,//sys_fallocate,
    [286] = 0,//sys_timerfd_settime,
    [287] = 0,//sys_timerfd_gettime,
    [288] = 0,//sys_accept4,
    [289] = 0,//sys_signalfd4,
    [290] = 0,//sys_eventfd2,
    [291] = 0,//sys_epoll_create1,
    [292] = 0,//sys_dup3,
    [293] = 0,//sys_pipe2,
    [294] = 0,//sys_inotify_init1,
    [295] = 0,//sys_preadv,
    [296] = 0,//sys_pwritev,
    [297] = 0,//sys_rt_tgsigqueueinfo,
    [298] = 0,//sys_perf_event_open,
    [299] = 0,//sys_recvmmsg,
    [300] = 0,//sys_fanotify_init,
    [301] = 0,//sys_fanotify_mark,
    [302] = 0,//sys_prlimit64,
    [303] = 0,//sys_name_to_handle_at,
    [304] = 0,//sys_open_by_handle_at,
    [305] = 0,//sys_clock_adjtime,
    [306] = 0,//sys_syncfs,
    [307] = 0,//sys_sendmmsg,
    [308] = 0,//sys_setns,
    [309] = 0,//sys_getcpu,
    [310] = 0,//sys_process_vm_readv,
    [311] = 0,//sys_process_vm_writev,
    [312] = 0,//sys_kcmp,
    [313] = 0//sys_finit_module
};
