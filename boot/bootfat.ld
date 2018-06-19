ENTRY(entry)

SECTIONS {
	.text 0x1000 : AT(0x1000) {
		___initialized_data_st = .;
		*(.head)

		. = 0x5C;
		*(.early)

		. = 0x1BE;
		*(.parttab)

		/* Reserve 512 bytes for FS Information sector */
		. = 0x400;

		___ctors_st = .;
		*(.ctors)
		*(.ctors.*)
		*(.init_array)
		*(SORT(.init_array.*))
		___ctors_en = .;

		___dtors_st = .;
		*(.dtors)
		*(.dtors.*)
		*(.fini_array)
		*(SORT(.fini_array.*))
		___dtors_en = .;

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

		___smp_st = .;
		*(.smp.entry)
		*(.smp.text)
		. = ALIGN(64);
		*(.smp.data)
		___smp_en = .;

		bootinfo_start = .;
		*(.bootinfo)

		/* sector align end */
		. = ALIGN(512);
		___initialized_data_en = .;
	} = 0

	.bss : {
		___bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)
		___bss_end = .;
	}

	___initial_stack_limit = .;
	. = ABSOLUTE(0xFFF0);
	___initial_stack = .;
	___heap_st = .;
	___end = .;

	/DISCARD/ : { *(.eh_frame) }
}
