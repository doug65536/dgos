#include "kmodule.h"

extern "C"
__attribute__((__section__(".entry")))
int module_entry(int argc, char **argv)
{
    return module_main(argc, argv);
}
