.section ".parttab"

# Geometry on a reasonable sized hard drive
.set geometry_cyl, 1023
.set geometry_head, 255
.set geometry_sec, 63

# Pack CHS into 3 byte bitfields
.macro partition_table_chs cyl,head,sec
	.byte \head
	.word (((\sec) & 0x3F) | ((\cyl) << 6))
.endm

.macro partition_table_lba_as_chs lba
	.if \lba >= (geometry_cyl * geometry_head * geometry_sec)
		partition_table_chs 1023,255,63
	.else
		.set partition_table_lba_as_chs_temp, ((\lba) / geometry_sec)
		.set partition_table_lba_as_chs_sec, (((\lba) % geometry_sec) + 1)
		.set partition_table_lba_as_chs_head, (partition_table_lba_as_chs_temp % geometry_head)
		.set partition_table_lba_as_chs_cyl, (partition_table_lba_as_chs_temp / geometry_head)
		partition_table_chs partition_table_lba_as_chs_cyl, partition_table_lba_as_chs_head, partition_table_lba_as_chs_sec
	.endif
.endm

.macro partition_table_entry active,lba_start,lba_sectors,type
	.if \active != 0
		.byte 0x80
	.else
		.byte 0x00
	.endif

	partition_table_lba_as_chs \lba_start

	.byte \type

	partition_table_lba_as_chs (\lba_start + \lba_sectors)

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

partition_table_entry 1,128,(1 << (33-9)),0x0b
partition_table_entry_unused
partition_table_entry_unused
partition_table_entry_unused

.word 0xAA55
