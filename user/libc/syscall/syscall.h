// syscall.h

long syscall6(long p0, long p1, long p2,
              long p3, long p4, long p5, long code);
long syscall5(long p0, long p1, long p2,
              long p3, long p4, long code);
long syscall4(long p0, long p1, long p2, long p3, long code);
long syscall3(long p0, long p1, long p2, long code);
long syscall2(long p0, long p1, long code);
long syscall1(long p0, long code);
long syscall0(long code);
