#pragma once
#include "thread.h"
#include "irq.h"
#include "pci.h"

int threaded_irq_request(int irq, int cpu, intr_handler_t handler_fn,
                         char const * devname);