// Careful, this is included in assembly code

// These two must be contiguous and in this order for syscall
#define GDT_SEL_KERNEL_CODE64   0x08
#define GDT_SEL_KERNEL_DATA     0x10
// These are arbitrary
#define GDT_SEL_KERNEL_CODE32   0x18
#define GDT_SEL_KERNEL_DATA32   0x20
#define GDT_SEL_KERNEL_CODE16   0x28
#define GDT_SEL_KERNEL_DATA16   0x30
// These three must be contiguous and in this order for sysret
#define GDT_SEL_USER_CODE32     0x38
#define GDT_SEL_USER_DATA       0x40
#define GDT_SEL_USER_CODE64     0x48
