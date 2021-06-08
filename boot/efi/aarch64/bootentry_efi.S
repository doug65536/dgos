
#include "../kernel/arch/aarch64/cfi_helpers.h"

.section .entry, "ax", @progbits
.global bootefi_entry
bootefi_entry:
    .cfi_startproc simple
    no_caller_cfi

    push_lr_fp

    // Store entry parameters globally
    adr x15,efi_image_handle
    adr x14,efi_systab
    str x3,[x15]
    str x2,[x14]

    // Run constructors
    bl ctors_invoke

    bl efi_main

    // Run destructors
    bl dtors_invoke

    pop_lr_fp

    ret
    .cfi_endproc

.section .data
.global efi_image_handle
efi_image_handle:
    .quad 0

.global efi_systab
efi_systab:
    .quad 0
