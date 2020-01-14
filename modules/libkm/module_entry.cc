#include "kmodule.h"

extern "C" char const ___eh_frame_st[];
extern "C" char const ___eh_frame_en[];

__attribute__((__visibility__("hidden")))
extern void *__dso_handle;

__attribute__((__visibility__("default")))
extern void const * const * __dso_handle_export;

__attribute__((__visibility__("default"),__used__))
void const * const * __dso_handle_export = &__dso_handle;

__attribute__((__visibility__("hidden")))
extern char const _eh_frame_hdr[];

__attribute__((__visibility__("hidden")))
extern char const _eh_frame_hdr_end[];

struct frame_registration {
    frame_registration()
    {
        if ((char const*)___eh_frame_en > (char const*)___eh_frame_st)
            __module_register_frame(&__dso_handle, (void*)___eh_frame_st,
                                    ___eh_frame_en - ___eh_frame_st);
    }

    ~frame_registration()
    {
        __module_unregister_frame(&__dso_handle, (void*)___eh_frame_st);
    }
};

extern "C"
__attribute__((__section__(".entry")))
int module_entry(int argc, char **argv)
{
    cpu_apply_fixups(___rodata_fixup_insn_st, ___rodata_fixup_insn_en);

    return module_main(argc, argv);
}

static frame_registration frame_registration_instance;
