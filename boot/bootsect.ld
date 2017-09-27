ENTRY(entry)

SECTIONS {
	.text 0x800 : AT(0x800) {
		__initialized_data_start = .;
		*(.head)

		. = 0x5A;
		*(.early)

		. = 0x1A0;
		SHORT(bootinfo_start);

		. = 0x1BE;
		*(.parttab)

		/* Reserve 512 bytes for FS Information sector */
		. = 0x400;

		__ctors_start = .;
		*(.ctors)
		*(.ctors.*)
		__ctors_end = .;

		/* --- additional sectors --- */
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)

		bootinfo_start = .;
		*(.bootinfo)

		/* sector align end */
		. = ALIGN(2048);
		__initialized_data_end = .;
	} = 0

	.bss : {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		__bss_end = .;

		. = ALIGN(16);
		__heap = .;
	}

	__initial_stack = 0xfff0;
	__heap_end = 0xd000;
}
