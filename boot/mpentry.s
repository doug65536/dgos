.code16
.section .text, "ax"

# This must be position independent code
# Note that this code is called with the cs set to
# some segment and ip is zero
.globl mp_entry
.globl mp_entry_size
mp_entry:
	cli
	cld
	xor %ax,%ax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%ss
	movl $0xFFF0,%esp
	
	lgdt gdtr
	
	# Enter protected mode
	mov %cr0,%eax
	bts $0,%eax
	mov %eax,%cr0
	
	xor %ecx,%ecx
	mov %cs,%ecx
	shl $4,%ecx
	
	# eax now holds address of mp_entry
	add $0f - mp_entry,%ecx
	pushl $0x18
	pushl %ecx
	data32 ljmp *(%esp)
	
0:
.code32
	movl $0x20,%ecx
	mov %ecx,%ds
	mov %ecx,%es
	mov %ecx,%fs
	mov %ecx,%gs
	mov %ecx,%ss
	movl mp_enter_kernel,%eax
	movl mp_enter_kernel+4,%edx
	mov $enter_kernel,%ecx
	jmp *%ecx
0:
	cli
	hlt
	jmp 0b

.align 8
mp_entry_size:
	.int mp_entry_size - mp_entry
