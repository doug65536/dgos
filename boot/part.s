.code16
.section .parttab, "ax", @progbits

.macro partition_table_entry active,lba_start,lba_sectors,type
	.if \active != 0
		.byte 0x80
	.else
		.byte 0x00
	.endif

	.byte 0
	.word 0

	.byte \type

	.byte 0
	.word 0

	.int \lba_start
	.int \lba_sectors
.endm

.macro partition_table_entry_unused
	.byte 0
	.byte 0
	.word 0
	.byte 0
	.byte 0
	.word 0
	.int 0
	.int 0
.endm

partition_table_entry 1,128,(1 << (24-9)),0x0C
partition_table_entry_unused
partition_table_entry_unused
partition_table_entry_unused

.word 0xAA55

