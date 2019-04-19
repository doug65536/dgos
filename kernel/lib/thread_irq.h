#pragma once
#include "thread.h"
#include "irq.h"
#include "pci.h"

int request_threaded_irq(pci_addr_t addr, int irq, int cpu,
                         intr_handler_t handler_fn, const char * devname);