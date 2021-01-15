#include "gdt_sel.h"
#include "gdt_macros.h"

.section .lowdata, "aw", @progbits

.global gdt
.global gdtr

gdt_emit gdt, gdtr

