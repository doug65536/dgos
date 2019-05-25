
// Retpoline. Works on the principle that the return stack overrides other
// branch prediction information. Cause it to mispredict into a pause loop
// until the ret retires, at which point it is guaranteed to branch to the
// correct destination. Attacker cannot train a mispredict to a malicious
// location.
.macro make_retpoline_thunk reg
.global __x86_indirect_thunk_\reg
.balign 16
__x86_indirect_thunk_\reg:
	.cfi_startproc
#ifdef __x86_64__
	.cfi_def_cfa rsp,8
	call 0f
1:  lfence
	pause
	jmp 1b
0:  .cfi_adjust_cfa_offset 8
	movq %\reg,(%rsp)
#elif defined(__i386__)
	.cfi_def_cfa esp,4
	call 0f
1:  lfence
	pause
	jmp 1b
0:  .cfi_adjust_cfa_offset 4
	movl %\reg,(%esp)
#endif
// This will speculatively return to the 1 label, but at retirement
// it will see that it should have branched to *%\reg
	ret
.cfi_endproc
.endm

#ifdef __x86_64__
.irp reg,rax,rcx,rdx,rbx,rbp,rsi,rdi,r8,r9,r10,r11,r12,r13,r14,r15
	make_retpoline_thunk \reg
.endr
#elif defined(__i386__)
.irp reg,eax,ebx,ecx,edx,esi,edi,ebp
	make_retpoline_thunk \reg
.endr
#endif
