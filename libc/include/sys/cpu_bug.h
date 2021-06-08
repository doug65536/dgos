#pragma once

#if defined(__x86_64__) || defined(__i386__)
#ifdef __ASSEMBLER__
#ifdef USE_RETPOLINE
.macro indirect_call reg
    call __x86_indirect_thunk_\reg
.endm
.macro indirect_jmp reg
    jmp __x86_indirect_thunk_\reg
.endm
#else
.macro indirect_call reg
    call *%\reg
.endm
.macro indirect_jmp reg
    jmp *%\reg
.endm
#endif
#endif
#endif

#ifdef __x86_64__

#define RAX %rax
#define RBX %rbx
#define RCX %rcx
#define RDX %rdx
#define RSI %rsi
#define RDI %rdi
#define RBP %rbp
#define RSP %rsp

// These must be used in reverse order for it to always do the right thing
// pass arg1 last
#define PASS_ARG6(arg) mov arg,%r9
#define PASS_ARG5(arg) mov arg,%r8
#define PASS_ARG4(arg) mov arg,%rdx
#define PASS_ARG3(arg) mov arg,%rcx
#define PASS_ARG2(arg) mov arg,%rsi
#define PASS_ARG1(arg) mov arg,%rdi

#define PTRSZ_DATA .quad

#elif defined(__i386__)

// For use when the pointer sized register is wanted
#define RAX %eax
#define RBX %ebx
#define RCX %ecx
#define RDX %edx
#define RSI %esi
#define RDI %edi
#define RBP %ebp
#define RSP %esp

// These must be used in reverse order for it to do the right thing
// pass arg1 last
#define PASS_ARG6(arg) push arg
#define PASS_ARG5(arg) push arg
#define PASS_ARG4(arg) push arg
#define PASS_ARG3(arg) push arg
#define PASS_ARG2(arg) push arg
#define PASS_ARG1(arg) push arg

#define PTRSZ_DATA .int

#endif
