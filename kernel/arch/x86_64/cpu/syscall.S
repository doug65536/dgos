.section .text, "ax", @progbits

#include "control_regs_constants.h"
#include "asm_constants.h"
#include "cfi_helpers.h"
#include "cpu_bug.h"
#include "fixup.h"

// i386 syscall ABI design notes
//  - syscall number in eax
//  - edx then ecx are preferred to be used first
//  - ebx, esi, edi, ebp must be preserved

// +------+--------+
// | i386 | x86_64 |
// +------+--------+
// |  eax |   eax  | syscall number
// | [ecx]|  [rcx] | return address \---+--> matches sysret instruction
// | [esp]|  [rsp] | caller's esp   /--/
// | xmm0 |   rdi  | 1st parameter
// | xmm1 |   rsi  | 2nd parameter
// | xmm2 |   rdx  | 3rd parameter
// | xmm3 |   rcx  | 4th parameter (r10 in syscall interface, rcx in function)
// | xmm4 |   r8   | 5th parameter
// | xmm5 |   r9   | 6th parameter
// +------+--------+

// incoming:
// eax syscall number, ecx return address, edx return stack pointer
// xmm0-xmm5 6 parameters in low 64 bits
// flags have the following bits cleared on entry by MSR_FMASK:
//  AC DF TF IF RF VM
// Must keep interrupts disabled until the xmm registers have been
// read because kernel preemptions don't save SSE state
.balign 16
.global syscall32_entry
.hidden syscall32_entry
syscall32_entry:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa_offset 0

     // CPU puts eip in ecx
    .cfi_register rip,rcx

    // esp still pointing to caller's stack
    .cfi_same_value rsp

    // Flags are in r11
    .cfi_register rflags,r11

    // CFI to encode caller's cs and ss to hardcoded values (because
    // we "know" cs and ss were certain values, since this is the
    // syscall code for 32 bit code
    .cfi_val_encoded_addr cs, 0, GDT_SEL_USER_CODE32+3
    .cfi_val_encoded_addr ss, 0, GDT_SEL_USER_DATA+3

    .cfi_remember_state

    // Switch to kernel gs
    swapgs

    // We can clobber r12, r13, r14, r15 without saving them,
    // because the caller is 32 bit code
    // Take advantage of that by packing the caller's
    // edi, esi, eip, esp in
    // r15, r14, r13, r12, avoiding the kernel stack entirely

    // Range check syscall number (range check whole register
    // because whole register is used in table lookup, even
    // though it is probably not possible to pass a 64 bit value)
    cmp $ SYSCALL_COUNT,%rax
    jae 0f

    // Try to get the parameters from SSE ASAP,
    // so we can reenable IRQs ASAP

    // Save edi in r13
    movl %edi,%r15d
    .cfi_register rdi,r15

    // Save esi in r14
    movl %esi,%r14d
    .cfi_register rsi,r14

    // Save eip in r13
    mov %ecx,%r13d
    .cfi_register rip,r13

    // Save esp in r12
    mov %esp,%r12d
    .cfi_register rsp,r12

    movq %xmm0,%rdi
    movq %xmm1,%rsi

    // Get pointer to current CPU's TSS from CPU-local data
    movq %gs:CPU_INFO_TSS_PTR_OFS,%r10

    movq %xmm2,%rdx
    movq %xmm3,%rcx

    // Get pointer to kernel stack from TSS (just like an interrupt)
    movq TSS_RSP0_OFS(%r10),%rsp
    .cfi_def_cfa rsp,0

    movq %xmm4,%r8
    movq %xmm5,%r9

    // IRQs are okay at this point, data has been retrieved
    // from SSE registers and stack pointer is on kernel stack
    sti

    // Save flags
    push_cfi %r11
    .cfi_offset rflags,-1*8

    // Read the syscall vector
    lea syscall_handlers(%rip),%r11
    movq (%r11,%rax,8),%rax

    // Call the syscall handler
    indirect_call rax

    // Restore flags
    pop_cfi %r11
    .cfi_register rflags,r11

    // Get return address back into ecx
    mov %r13d,%ecx
    .cfi_register rip,rcx

    // Restore clobbered callee saved r14, r15

    mov %r15d,%edi
    .cfi_same_value rdi

    mov %r14d,%esi
    .cfi_same_value rsi

    // Interrupts not safe while switched back to user stack in kernel
    cli

    mov %r12,%rsp

    // Exactly like state at syscall entry below here
    .cfi_restore_state

    // Put 64 bit return value into edx:eax
1:  movq %rax,%rdx
    shrq $ 32,%rdx

    insn_fixup
    call protection_barrier_to_user

    swapgs
    sysretl

0:  movq $ SYSCALL_ENOSYS,%rax
    jmp 1b
    lfence

    .cfi_endproc

// This code may clobber rdi, rsi, rdx, rcx, r8-r11, rflags
// This code may clobber the entire FPU/SSE/AVX state (except control words)
// This code must preserve rbx, rbp, r12-r15
// Note that these are the same rules as x86_64-elf function calls
// eflags has the following bits cleared on entry by MSR_FMASK:
//  AC DF TF IF RF VM

.balign 16
.global syscall_entry
.hidden syscall_entry
syscall_entry:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa_offset 0

    // CPU puts rip in rcx
    .cfi_register rip,rcx

    // rsp still pointing the caller's stack!
    .cfi_register rsp,rsp

    // CFI to encode caller's cs and ss to hardcoded values (because
    // we "know" cs and ss were certain values, since this is the
    // syscall code for 64 bit code
    .cfi_val_encoded_addr cs, 0, GDT_SEL_USER_CODE64+3
    .cfi_val_encoded_addr ss, 0, GDT_SEL_USER_DATA+3

    // syscall rax
    // params rdi, rsi, rdx, r10, r8, r9
    // return rax
    // CPU puts rflags in r11
    // on return, rflags is initialized to CPU_EFLAGS_IF | 2

    // Switch to kernel gs immediately
    swapgs

    // Range check syscall number
    cmpq $ SYSCALL_COUNT,%rax
    jae 0f

    // Save flags
    mov %r11d,%gs:CPU_INFO_SYSCALL_FLAGS_OFS

    // Read function pointer from vector table
    lea syscall_handlers(%rip),%r11
    movq (%r11,%rax,8),%rax

    // Get pointer to current CPU's TSS from CPU-local data
    movq %gs:CPU_INFO_TSS_PTR_OFS,%r11

    // Get privilege change stack from TSS data (just like an interrupt would)
    movq TSS_RSP0_OFS(%r11),%r11

    // Switch to this thread's kernel stack
    xchgq %rsp,%r11

    .cfi_def_cfa r11,0
    .cfi_register rsp,r11

    // Push user stack pointer to syscall stack
    pushq %r11
    .cfi_def_cfa rsp,8
    .cfi_offset rsp,-1*8

    // Push flags to syscall stack
    push_cfi %gs:CPU_INFO_SYSCALL_FLAGS_OFS
    .cfi_offset eflags,-2*8

    // Push return address to syscall stack
    push_cfi %rcx
    .cfi_offset rip,-3*8

    // call IBPB or patched with nop if not available
    insn_fixup
    call protection_barrier_from_user

    // IRQs are okay at this point
    sti

    // Move 4th parameter to proper place
    movq %r10,%rcx

    // Call handler
    indirect_call rax

    xorl %edx,%edx
    movl %edx,%esi
    movl %edx,%edi
    movl %edx,%r8d
    movl %edx,%r9d
    movl %edx,%r10d

    // IRQs are not safe when
    // - we have user gs and still in kernel mode
    // - stack is switched to user stack in kernel mode
    cli

    // Restore return address
    pop_cfi %rcx
    .cfi_register rip,rcx

    // Restore return flags
    pop_cfi %r11

    insn_fixup
    call protection_barrier_to_user

    // Restore caller's stack
    pop_cfi %rsp
    .cfi_def_cfa rsp,0
    .cfi_register rsp,rsp

    // Switch to user gs
    swapgs
    sysretq

    // syscall number out of range
0:  movq $ SYSCALL_ENOSYS,%rax
    swapgs
    sysretq

    .cfi_endproc

.balign 16
.global syscall_entry_end
.hidden syscall_entry_end
syscall_entry_end:
