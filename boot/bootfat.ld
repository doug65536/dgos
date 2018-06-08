ENTRY(entry)

SECTIONS {
	.text 0x1000 : AT(0x1000) {
		__initialized_data_start = .;
		*(.head)

		. = 0x5C;
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
		*(.init_array)
		*(SORT(.init_array.*))
		__ctors_end = .;

		/* --- additional sectors --- */
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		/* Place 64 bit code high enough to be outside first 4KB */
		*(.text.code64)

		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)

		bootinfo_start = .;
		*(.bootinfo)

		/* sector align end */
		. = ALIGN(512);
		__initialized_data_end = .;
	} = 0

	.bss : {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)
		__bss_end = .;
	}

	.uninit : {
		*(.stack)

		. = ALIGN(16);
		__heap_st = .;
	}

	/DISCARD/ : { *(.eh_frame) }
}
