
// Win32 x86_64 ABI requires the following registers are preserved by callee
//  rbx, rsi, rdi, rbp, r12, r13, r14, r15

// The x86_64 ABI ELF requires the following registers are preserved by callee
//  rbx, rbp, r12, r13, r14, r15

.code64
.section .entry, "ax", @progbits
.global bootefi_entry
bootefi_entry:
    .cfi_startproc
    .cfi_undefined rip
    .cfi_undefined rsp
    .cfi_undefined rbx
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15

    pushq %rbp
    .cfi_adjust_cfa_offset 8
    .cfi_offset rbp,-2*8

    movq %rsp,%rbp
    .cfi_register rsp,rbp

    // Win32 x86_64 ABI caller is expecting rsi, rdi preserved
    pushq %rsi
    .cfi_adjust_cfa_offset 8
    .cfi_offset rsi,-3*8

    pushq %rdi
    .cfi_adjust_cfa_offset 8
    .cfi_offset rdi,-4*8

    push %rbx
    .cfi_adjust_cfa_offset 8
    .cfi_offset rbx,-5*8

    // Realign stack in case of nonsense incoming alignment
    mov %rsp,%rbx
    andq $ -16,%rsp

    // Store entry parameters globally
    movq %rcx,efi_image_handle(%rip)
    movq %rdx,efi_systab(%rip)

    // Run constructors
    callq ctors_invoke

    callq efi_main

    // Run destructors
    callq dtors_invoke

    // Undo stack alignment
    mov %rbx,%rsp

    pop %rbx
    .cfi_adjust_cfa_offset -8
    .cfi_same_value rbx

    // Win32 x86_64 ABI caller is expecting rsi, rdi preserved
    // Restore caller rsi, rdi
    popq %rdi
    .cfi_adjust_cfa_offset -8
    .cfi_same_value rdi

    popq %rsi
    .cfi_adjust_cfa_offset -8
    .cfi_same_value rsi

    leaveq
    .cfi_adjust_cfa_offset -8
    .cfi_same_value rbp

    retq
    .cfi_endproc

.section .data
.global efi_image_handle
efi_image_handle:
    .quad 0

.global efi_systab
efi_systab:
    .quad 0
