.text

.global syscall_entry
.hidden syscall_entry
syscall_entry:
	# syscall rax
	# params rdi, rsi, rdx, r10, r8, r9
	# return rax

	# Range check syscall number
	cmp $314,%rax
	jae 0f

	# CPU puts rflags in r11
	push %r11

	# Scheduled ahead
	lea (,%rax,8),%r11

	# Get base address of syscall vector table
	lea syscall_handlers(%rip),%rax

	# Add syscall table offset to address of syscall_handlers
	# Read function pointer from vector table
	mov (%r11,%rax),%rax

	# Validate non-null
	test %rax,%rax
	jz 1f

	swapgs

	# CPU puts return address in rcx
	push %rcx

	# Save stack pointer in rbx
	push %rbx
	mov %rsp,%rbx

	# Get pointer to current thread from CPU data
	mov %gs:8,%rcx

	# Get pointer to syscall stack from thread data
	mov 8(%rcx),%rsp

	# Move 4th parameter to proper place
	mov %r10,%rcx

	# Enforce ABI
	cld

	# Dummy push to align stack
	push %rax

	# Call handler
	call *%rax

	swapgs

	# Restore caller stack
	mov %rbx,%rsp

	# Restore call saved register
	pop %rbx

	# Restore return address
	pop %rcx

	# Restore flags
	pop %r11

	sysret

1:	# Vector table contained null pointer
	pop %r11

	# ... fall through ...

0:	# syscall number out of range
	# 38 is ENOSYS
	mov $38,%rax
	sysret
