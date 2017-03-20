.section .entry, "ax"

.global entry
.hidden entry
entry:
	cld
	xor %ebp,%ebp

	# Store the physical memory map address
	# passed in from bootloader
	mov %ecx,%edx
	shr $20,%edx
	mov %rdx,phys_mem_map_count
	and $0x000FFFFF,%rcx
	mov %rcx,phys_mem_map

	# Enable SSE (CR4_OFXSR_BIT) and SSE exceptions CR4_OSXMMEX)
	# This must be done before jumping into C code
	mov %cr4,%rax
	or $0x600,%rax
	mov %rax,%cr4

	# Get the MXCSR_MASK
	mov %rsp,%rdx
	sub $512,%rsp
	and $-64,%rsp

	fxsave64 (%rsp)

	mov 28(%rsp),%eax
	mov %eax,default_mxcsr_mask

	# Set MXCSR to 64-bit precision,
	# all exceptions masked, round to nearest
	movl $((3 << 13) | (0x3F << 7)),24(%rsp)
	ldmxcsr 24(%rsp)

	mov %rdx,%rsp

	# Initialize FPU to 64 bit precision,
	# all exceptions masked, round to nearest
	fninit
	push $((3 << 10) | 0x3F)
	fldcw (%rsp)
	add $8,%rsp

	push %rdx
	push %rcx

	# See if this is the bootstrap processor
	mov $0x1B,%ecx
	rdmsr
	pop %rcx
	pop %rdx
	test $0x100,%eax
	jnz 0f

	# Align stack
	xor %ebp,%ebp
	push $0

	# MP processor entry
	jmp mp_main

0:

	lea kernel_stack,%rdx
	mov kernel_stack_size,%rbx

	mov %rdx,%rdi
	mov %rbx,%rcx
	mov $0xcc,%al
	rep stosb

	lea (%rdx,%rbx),%rsp

	call e9debug_init

	xor %edi,%edi
	call cpu_init

	# Call the constructors
	lea ___init_st(%rip),%rdi
	lea ___init_en(%rip),%rsi
	call invoke_function_array

	xor %edi,%edi
	call cpu_hw_init

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
	pop %rbx
	ret

# Callout to initialize AP CPU
.global mp_main
.hidden mp_main
mp_main:
	mov $'S',%edi
	call callout_call
	ret
