#pragma once

.macro struct_begin name
    .struct 0
    \name\()_start:
.endm

.macro struct_field size name
    \name\(): .struct \name + \size
.endm

.macro struct_end name
    \name\()_end:
    \name\()_length = \name\()_end - \name\()_start
.endm
