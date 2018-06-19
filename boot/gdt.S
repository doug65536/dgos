#include "gdt_sel.h"
#include "gdt_macros.h"

.section .data

.global gdt
.global gdtr

gdt_emit gdt, gdtr

