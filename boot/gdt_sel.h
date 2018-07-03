// Careful, this is included in assembly code

#define GDT_SEL_PM_CODE16       0x08
#define GDT_SEL_PM_DATA16       0x10
#define GDT_SEL_PM_CODE32       0x18
#define GDT_SEL_PM_DATA32       0x20

#define GDT_SEL_USER_CODE32     0x40
#define GDT_SEL_USER_DATA       0x48
#define GDT_SEL_USER_CODE64     0x50

#define GDT_SEL_KERNEL_CODE64   0x60
#define GDT_SEL_KERNEL_DATA     0x68

#define GDT_SEL_TSS             0x80
#define GDT_SEL_TSS_HI          0x88

//
// Unused descriptors

// 3 consecutive descriptors
#define GDT_SEL_AVAIL_0         0x28
#define GDT_SEL_AVAIL_1         0x30
#define GDT_SEL_AVAIL_2         0x38

// Single descriptor
#define GDT_SEL_AVAIL_3         0x58

// 10 consecutive descriptors
#define GDT_SEL_AVAIL_4         0x70
#define GDT_SEL_AVAIL_5         0x78
#define GDT_SEL_AVAIL_6         0x80
#define GDT_SEL_AVAIL_7         0x88
#define GDT_SEL_AVAIL_8         0x90
#define GDT_SEL_AVAIL_9         0x98
#define GDT_SEL_AVAIL_10        0xA0
#define GDT_SEL_AVAIL_11        0xA8
#define GDT_SEL_AVAIL_12        0xB0
#define GDT_SEL_AVAIL_13        0xB8

#define GDT_SEL_END             0xC0

#define GDT_TYPE_TSS            0x09
