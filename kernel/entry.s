.globl entry
entry:
	call main
0:
	cli
	hlt
	jmp 0b
