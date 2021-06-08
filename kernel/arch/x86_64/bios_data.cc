#include "bios_data.h"
#include "mm.h"
#include "callout.h"

extern HIDDEN char *zero_page;

static void biosdata_remap(void *arg)
{
    (void)arg;
    zero_page = (char*)mmap(
            nullptr, 64 << 10, PROT_READ | PROT_WRITE,
            MAP_PHYSICAL);
    assert(zero_page != MAP_FAILED);
}

REGISTER_CALLOUT(biosdata_remap, nullptr,
                 callout_type_t::vmm_ready, "000");
