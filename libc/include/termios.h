#pragma once

//
// Output Modes

#define OPOST 0x00000001
#define ONCLR 0x00000002
#define OCRNL 0x00000004
#define ONOCR 0x00000008
#define OFILL 0x00000010
#define OFDEL 0x00000020
#define NLDLY 0x00000040
#define  NL0  0x00000080
#define  NL1  0x00000100
#define CRDLY 0x00000200
#define  CR0  0x00000400
#define  CR1  0x00000800
#define  CR2  0x00001000
#define  CR3  0x00002000
#define BSDLY 0x00004000
#define  BS0  0x00008000
#define  BS1  0x00010000
#define VTDLY 0x00020000
#define  VT0  0x00040000
#define  VT1  0x00080000
#define FFDLY 0x00100000
#define  FF0  0x00200000
#define  FF1  0x00400000

//
// Control Modes

#define CLOCAL      0x00000001
#define CREAD       0x00000002
#define CSIZE       0x00000004
#define CS5         0x00000008
#define CS6         0x00000010
#define CS7         0x00000020
#define CS8         0x00000040
#define CSTOPB      0x00000080
#define HUPCL       0x00000100
#define PARENB      0x00000200
#define PARODD      0x00000400
#define B0          0x00000800
#define B50         0x00001000
#define B75         0x00002000
#define B110        0x00004000
#define B134        0x00008000
#define B150        0x00010000
#define B200        0x00020000
#define B300        0x00040000
#define B600        0x00080000
#define B1200       0x00100000
#define B1800       0x00200000
#define B2400       0x00400000
#define B4800       0x00800000
#define B9600       0x01000000
#define B19200      0x02000000
#define B38400      0x04000000
#define B57600      0x08000000
#define B115200     0x10000000
#define B230400     0x20000000
#define B460800     0x40000000
#define B921600     0x80000000

// c_iflag
#define IGNBRK      0x00000001
#define BRKINT      0x00000002
#define IGNPAR      0x00000004
#define PARMRK      0x00000008
#define INPCK       0x00000010
#define ISTRIP      0x00000020
#define INLCR       0x00000040
#define IGNCR       0x00000080
#define ICRNL       0x00000100
#define IUCLC       0x00000200
#define IXON        0x00000400
#define IXANY       0x00000800
#define IXOFF       0x00001000
#define IMAXBEL     0x00002000
#define IUTF8       0x00004000

//
// Local Modes

#define ECHO    0x001
#define ECHOE   0x002
#define ECHOK   0x004
#define ECHONL  0x008
#define ICANON  0x010
#define IEXTEN  0x020
#define ISIG    0x040
#define NOFLSH  0x080
#define TOSTOP  0x100

typedef unsigned tcflag_t;
typedef unsigned speed_t;
typedef unsigned char cc_t;

#define NCCS 32

struct termios {
    // Input mode flags
    tcflag_t c_iflag;

    // Output mode flags
    tcflag_t c_oflag;

    // Control mode flags
    tcflag_t c_cflag;

    // Local mode flags
    tcflag_t c_lflag;

    // Line discipline
    cc_t c_line;

    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define _HAVE_STRUCT_TERMIOS_C_ISPEED 1
#define _HAVE_STRUCT_TERMIOS_C_OSPEED 1

#define VINTR       0
#define VQUIT       1
#define VERASE      2
#define VKILL       3
#define VEOF        4
#define VTIME       5
#define VMIN        6
#define VSWTC       7
#define VSTART      8
#define VSTOP       9
#define VSUSP       10
#define VEOL        11
#define VREPRINT    12
#define VDISCARD    13
#define VWERASE     14
#define VLNEXT      15
#define VEOL2       16
