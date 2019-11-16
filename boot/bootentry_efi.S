
// Win32 x86_64 ABI requires the following registers are preserved by callee
//  rbx, rsi, rdi, rbp, r12, r13, r14, r15

// The x86_64 ABI ELF requires the following registers are preserved by callee
//  rbx, rbp, r12, r13, r14, r15

.code64
.section .entry, "ax", @progbits
.global entry
entry:
    .cfi_startproc

    pushq %rbp
    .cfi_adjust_cfa_offset 8

    movq %rsp,%rbp
    .cfi_register rsp,rbp

    // Win32 x86_64 ABI caller is expecting rsi, rdi preserved
    pushq %rsi
    pushq %rdi

    // Move MS ABI parameter registers into ELF ABI parameter registers
    movq %rcx,efi_image_handle(%rip)
    movq %rdx,efi_systab(%rip)

    // Run constructors
    callq ctors_invoke

    callq efi_main

    // Run destructors
    callq dtors_invoke

    // Win32 x86_64 ABI caller is expecting rsi, rdi preserved
    // Restore caller rsi, rdi
    popq %rdi
    popq %rsi

    leaveq
    .cfi_same_value rbp
    .cfi_register rsp,rsp

    retq
    .cfi_endproc
