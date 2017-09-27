ENTRY(entry)
SECTIONS {
	.text 0x600 : AT(0x600) {
		*(.text)
		*(.data)
		*(.bss)

		. = 0x1BE;
		partition_table = .;
		*(.parttab)

		. = 512;
	} = 0
}
