
typedef long syscall_handler_t(long p0, long p1, long p2,
                                  long p3, long p4, long p5);

extern syscall_handler_t *syscall_handlers[];
