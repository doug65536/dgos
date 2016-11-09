.section .entry

.globl entry
entry:
	jmp 1f
	# Debugger hack
	mov $0,%rbp
	0:
	test %rbp,%rbp
	pause
	jz 0b
	1:

	# Store the physical memory map address
	# passed in from bootloader
	mov %rcx,phys_mem_map(%rip)

	# Align stack
	and $-16,%rsp

	call cpu_init

	call tls_init

	# Call the constructors
	lea ___init_st(%rip),%rdi
	lea ___init_en(%rip),%rsi
	call invoke_function_array

	call main

	mov %rax,%rdi
	call exit

.globl exit
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
