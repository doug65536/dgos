#pragma once
#include "thread.h"
#include "irq.h"

int request_threaded_irq(unsigned int irq, intr_handler_t handler,
    irq_handler_t thread_fn,
    unsigned long irqflags,
    const char * devname,
    void * dev_id);