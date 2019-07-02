#include "kmodule.h"

extern "C" char const ___eh_frame_st[];
extern "C" char const ___eh_frame_en[];
extern "C" void __register_frame(char const *frames);

extern void const * const __dso_handle;

extern "C"
__attribute__((__section__(".entry")))
int module_entry(int argc, char **argv)
{
    if ((char const*)___eh_frame_en > (char const*)___eh_frame_st)
        __module_register_frame(&__dso_handle, (void*)___eh_frame_st);

    return module_main(argc, argv);
}
