ENTRY(entry)

SECTIONS {
	.text 0x1000 : AT(0x1000) {
		___initialized_data_st = .;
		*(.head)
		*(.early)

		/* --- additional sectors --- */
		*(.text)
		*(.text.*)
		*(.text.unlikely)
		
		. = ALIGN(4);
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
	. = ABSOLUTE(0xF000);
	___initial_stack = .;
	___heap_st = .;
	___end = .;

	/DISCARD/ : { *(.eh_frame) }
}
