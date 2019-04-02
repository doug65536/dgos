#include <sys/module.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int probe_pci_for(
        int32_t vendor,
        int32_t device,
        int32_t devclass,
        int32_t subclass,
        int32_t prog_if)
{
    long result = syscall5(
                long(vendor),
                long(device),
                long(devclass),
                long(subclass),
                long(prog_if),
                SYS_probe_pci_for);

    if (likely(result >= 0))
        return int(result);

    errno = -result;

    return -1;
}
