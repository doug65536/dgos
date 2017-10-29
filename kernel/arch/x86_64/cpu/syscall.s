.section .text, "ax"

.global syscall_entry
.hidden syscall_entry
syscall_entry:
	# syscall rax
	# params rdi, rsi, rdx, r10, r8, r9
	# return rax

	# Range check syscall number
	cmpq $314,%rax
	jae 0f

	# CPU puts rflags in r11
	pushq %r11

	# Scheduled ahead
	leaq (,%rax,8),%r11

	# Get base address of syscall vector table
	leaq syscall_handlers(%rip),%rax

	# Add syscall table offset to address of syscall_handlers
	# Read function pointer from vector table
	movq (%r11,%rax),%rax

	# Validate non-null
	testq %rax,%rax
	jz 1f

	swapgs

	# CPU puts return address in rcx
	pushq %rcx

	# Save stack pointer in rbx
	pushq %rbx
	movq %rsp,%rbx

	# Get pointer to current thread from CPU data
	movq %gs:8,%rcx

	# Get pointer to syscall stack from thread data
	movq 8(%rcx),%rsp

	# Move 4th parameter to proper place
	movq %r10,%rcx

	# Enforce ABI
	cld

	# Dummy push to align stack
	pushq %rax

	# Call handler
	callq *%rax

	swapgs

	# Restore caller stack
	movq %rbx,%rsp

	# Restore call saved register
	popq %rbx

	# Restore return address
	popq %rcx

	# Restore flags
	popq %r11

	sysretq

1:	# Vector table contained null pointer
	popq %r11

	# ... fall through ...

0:	# syscall number out of range
	# 38 is ENOSYS
	movq $-38,%rax
	sysretq
