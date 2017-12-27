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

#define GDT_SEL_END             0xC0

#define GDT_TYPE_TSS            0x09
