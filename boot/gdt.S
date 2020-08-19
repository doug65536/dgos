#include "gdt_sel.h"
#include "gdt_macros.h"

.section .low, "aw", @progbits

.global gdt
.global gdtr

gdt_emit gdt, gdtr

