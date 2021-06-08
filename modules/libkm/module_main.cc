#include "kmodule.h"
#include "printk.h"

__attribute__((__weak__, __visibility__("hidden")))
int module_main(int argc, char const * const * argv)
{
    printk("Started module %s\n", argv[0]);
    return 0;
}
