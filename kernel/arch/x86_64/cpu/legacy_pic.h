#pragma once
#include "isr.h"

void pic8259_enable(void);
void pic8259_disable(void);

extern "C" isr_context_t *pic8259_dispatcher(
        int intr, isr_context_t *ctx);
