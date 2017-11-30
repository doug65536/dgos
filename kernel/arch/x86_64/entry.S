#include "cpu/control_regs_constants.h"

.section .entry, "ax"

.macro debugind ch
	movb $\ch,0xb8000
.endm

// Things that must be done before entering any compiler
// generated code:
//  - Set up stack (aligned!)
//  - Enable SSE
//  - Initialize SSE control word (mxcsr)
//  - Initialize x87 control word
//  - Initialize SSE control word mask
//  - Initialize xsave

.align 16
.global entry
.hidden entry
.type entry,@function
entry:
	debugind '1'

	// Align the stack
	and $-16,%rsp

	// Save parameter in call-preserved register
	mov %ecx,%r15d
	xor %ebp,%ebp

	// Enable SSE
	// This must be done before jumping into C code
	mov %cr4,%rax
	or $(CR4_OFXSR | CR4_OSXMMEX),%rax
	mov %rax,%cr4

	debugind '2'

	// See if this is the bootstrap processor
	mov $0x1B,%ecx
	rdmsr
	test $0x100,%eax
	jnz 0f

	debugind 'Z'

	// This is not the bootstrap CPU

	// Initialize AP MXCSR
	mov $(MXCSR_ELF_INIT),%eax
	and default_mxcsr_mask,%eax
	push %rax
	ldmxcsr (%rsp)

	// Initialize AP FPU control word
	// Round nearest, 53 bit significand, all exceptions masked
	movw $(FPUCW_ELF_INIT),(%rsp)
	fldcw (%rsp)

	pop %rax

	// xsave-enable AVX on AP
	call set_xsave_states_ap

	// Align stack
	xor %ebp,%ebp
	push %rbp

	// MP processor entry
	jmp mp_main

0:
	// This is the bootstrap processor

	debugind '3'

	//Fill stack with garbage
	//mov %rdx,%rdi
	//mov %rbx,%rcx
	//mov $0xcc,%al
	//cld
	//rep stosb

	lea kernel_stack,%rdx
	mov kernel_stack_size,%rbx
	lea (%rdx,%rbx),%rsp

	// Store the physical memory map address
	// passed in from bootloader
	mov %r15d,%eax
	shr $20,%eax
	mov %rax,phys_mem_map_count

	mov %r15d,%eax
	and $0x000FFFFF,%eax
	mov %rax,phys_mem_map

	// Save the MXCSR_MASK

	// Allocate 512 bytes and cache line align stack
	mov %rsp,%rdx
	sub $512,%rsp
	and $-64,%rsp

	// Initialize FPU to 64 bit precision,
	// all exceptions masked, round to nearest
	fninit
	movw $((0 << 10) | (2 << 8) | 0x3F),(%rsp)
	fldcw (%rsp)

	fxsave64 (%rsp)

	// Read MXCSR_MASK from fxsave output and store it
	mov 28(%rsp),%eax
	mov %eax,default_mxcsr_mask

	// Set MXCSR
	// 0x3F = all exceptions masked
	// 0 = round to nearest
	mov $(MXCSR_RC_n(MXCSR_RC_NEAREST) | MXCSR_MASK_ALL),%ecx
	and %eax,%ecx
	mov %ecx,24(%rsp)
	ldmxcsr 24(%rsp)

	// Restore stack pointer
	mov %rdx,%rsp

	debugind '4'
	call cpuid_init

	debugind '6'

	// Must xsave-enable AVX early if available
	call detect_xsave_states_bsp

	// Get SSE context offsets
	xor %edi,%edi
	call idt_xsave_detect

	// Call the constructors
	mov $___init_st,%rdi
	mov $___init_en,%rsi
	call invoke_function_array

	// Notify constructors ran
	mov $'C',%edi
	call callout_call

	debugind '7'

	xor %edi,%edi
	call cpu_init

	call e9debug_init

	debugind '8'

	// Initialize text devices
	mov $'V',%edi
	call callout_call

	xor %edi,%edi
	call cpu_init_stage2

	debugind '9'

	xor %edi,%edi
	call cpu_hw_init

	debugind 'A'

	// Initialize GDB stub
	//call gdb_init

	// Initialize early-initialized devices
	mov $'E',%edi
	call callout_call

	debugind 'B'

	call main

	debugind '?'

	mov %rax,%rdi
	call exit

.align 16
.global exit
.hidden exit
exit:
	// Ignore exitcode
	// Kernel exit just calls destructors
	// and deliberately hangs
0:
	lea ___fini_st(%rip),%rdi
	lea ___fini_en(%rip),%rsi
	call invoke_function_array

	call halt_forever

.align 16
invoke_function_array:
	push %rbx
	push %rbp
	push %rbp
	mov %rdi,%rbx
	mov %rsi,%rbp
0:
	cmp %rbx,%rbp
	jbe 0f
	call *(%rbx)
	add $8,%rbx
	jmp 0b
0:
	pop %rbp
	pop %rbp
	pop %rbx
	ret

#define XSAVE_WANTED \
	XCR0_X87 | \
	XCR0_SSE | XCR0_AVX | \
	XCR0_AVX512_OPMASK | \
	XCR0_AVX512_UPPER | \
	XCR0_AVX512_XREGS

.align 16
detect_xsave_states_bsp:
	push %rbx
	call cpuid_has_xsave
	test %al,%al
	jz 0f

	mov $CPUID_INFO_XSAVE,%eax
	xor %ecx,%ecx
	cpuid

	mov %eax,xsave_supported_states
	and $XSAVE_WANTED,%eax
	mov %eax,xsave_enabled_states

0:
	pop %rbx
	jmp set_xsave_states_ap

.align 16
set_xsave_states_ap:
	call cpuid_has_xsave
	test %al,%al
	jz 0f

	mov %cr4,%rax
	or $CR4_OSXSAVE,%rax
	mov %rax,%cr4

	xor %ecx,%ecx
	xgetbv
	and xsave_supported_states,%eax
	or xsave_enabled_states,%eax
	xsetbv

0:
	ret

// Callout to initialize AP CPU
.align 16
.global mp_main
.hidden mp_main
mp_main:
	push %rbp
	mov %rsp,%rbp

	mov $'S',%edi
	call callout_call

	pop %rbp
	ret

.global __cxa_pure_virtual
.hidden __cxa_pure_virtual
__cxa_pure_virtual:
	mov $pure_call_message,%rdi
	jmp panic

.align 16
.global __cxa_atexit
__cxa_atexit:
	ret

.section .rodata
pure_call_message:
	.string "Pure virtual function called"

.global __dso_handle
__dso_handle: .quad __dso_handle
