#include "printk.h"

char x[1024];
int chkabs;
int *chkabsptr = &chkabs;
void (*chk)(char const *f, ...) = printk;

int entry(void)
{
    printk("Yay! chkabsptr = %p\n", (void*)chkabsptr);
}
