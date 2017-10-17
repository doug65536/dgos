.section .entry, "ax"

.global entry
.hidden entry
entry:
	cld

	# Save parameter in call-preserved register
	mov %ecx,%r15d
	xor %ebp,%ebp

	# Enable SSE (CR4_OFXSR_BIT) and SSE exceptions (CR4_OSXMMEX)
	# and RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE (CR4_FSGSBASE_BIT)
	# This must be done before jumping into C code
	mov %cr4,%rax
	or $0x10600,%rax
	mov %rax,%cr4

	# See if this is the bootstrap processor
	mov $0x1B,%ecx
	rdmsr
	test $0x100,%eax
	jnz 0f

	# This is not the bootstrap CPU

	# Initialize MXCSR
	mov $((3 << 13) | (0x3F << 7)),%eax
	and default_mxcsr_mask,%eax
	push %rax
	ldmxcsr (%rsp)

	# Initialize FPU control word
	movq $((3 << 10) | 0x3F),(%rsp)
	fldcw (%rsp)

	pop %rax

	# xsave-enable AVX on AP
	mov $1,%edi
	call idt_xsave_detect

	# Align stack
	xor %ebp,%ebp
	push %rbp

	# MP processor entry
	jmp mp_main

0:
	# This is the bootstrap processor

	# Store the physical memory map address
	# passed in from bootloader
	mov %r15d,%eax
	shr $20,%eax
	mov %rax,phys_mem_map_count

	mov %r15d,%eax
	and $0x000FFFFF,%eax
	mov %rax,phys_mem_map

	# Save the MXCSR_MASK

	# Allocate 512 bytes and cache line align stack
	mov %rsp,%rdx
	sub $512,%rsp
	and $-64,%rsp

	fxsave64 (%rsp)

	# Read MXCSR_MASK from fxsave output and store it
	mov 28(%rsp),%eax
	mov %eax,default_mxcsr_mask

	# Set MXCSR to 64-bit precision,
	# all exceptions masked, round to nearest
	mov $((3 << 13) | (0x3F << 7)),%ecx
	and %eax,%ecx
	mov %ecx,24(%rsp)
	ldmxcsr 24(%rsp)

	# Free 512 bytes
	mov %rdx,%rsp

	# Initialize FPU to 64 bit precision,
	# all exceptions masked, round to nearest
	fninit
	push $((3 << 10) | 0x3F)
	fldcw (%rsp)
	add $8,%rsp

	lea kernel_stack,%rdx
	mov kernel_stack_size,%rbx

	mov %rdx,%rdi
	mov %rbx,%rcx
	mov $0xcc,%al
	rep stosb

	lea -16(%rdx,%rbx),%rsp

	call cpuid_init

	call e9debug_init

	# Must xsave-enable AVX ASAP if available
	xor %edi,%edi
	call idt_xsave_detect

	xor %edi,%edi
	call cpu_init

	# Call the constructors
	mov $___init_st,%rdi
	mov $___init_en,%rsi
	call invoke_function_array

	xor %edi,%edi
	call cpu_init_stage2

	xor %edi,%edi
	call cpu_hw_init

	# Initialize GDB stub
	#call gdb_init

	# Initialize early-initialized devices
	mov $'E',%edi
	call callout_call

	call main

	mov %rax,%rdi
	call exit

.global exit
.hidden exit
exit:
	# Ignore exitcode
	# Kernel exit just calls destructors
	# and deliberately hangs
0:
	lea ___fini_st(%rip),%rdi
	lea ___fini_en(%rip),%rsi
	call invoke_function_array

	call halt_forever

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

# Callout to initialize AP CPU
.global mp_main
.hidden mp_main
mp_main:
	mov $'S',%edi
	call callout_call
	ret

.global __cxa_pure_virtual
.hidden __cxa_pure_virtual
__cxa_pure_virtual:
	mov $pure_call_message,%rdi
	jmp panic

.global __cxa_atexit
__cxa_atexit:
	ret

.section .rodata
pure_call_message:
	.string "Pure virtual function called"

.global __dso_handle
__dso_handle: .quad __dso_handle
