#include "thread_irq.h"

int request_threaded_irq(pci_addr_t addr, int irq, int cpu,
                         intr_handler_t handler_fn, char const *devname)
{
    return -int(errno_t::ENOSYS);
}
