ENTRY(entry)

/* NOTE: only the first 4KB is loaded by the BIOS, */
/* in case a dumbass BIOS has its stack in the 1st 64KB (seabios), */
/* the startup code loads the rest */

SECTIONS {
	.text 0x1000 : AT(0x1000) {
		___initialized_data_st = .;
		*(.head)
		*(.early)
		
		. = ALIGN(4096);

		. = ALIGN(64);
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		. = ALIGN(64);
		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)

		. = ALIGN(16);
		___ctors_st = .;
		*(.ctors)
		*(.ctors.*)
		*(.init_array)
		*(SORT(.init_array.*))
		___ctors_en = .;

		. = ALIGN(16);
		___dtors_st = .;
		*(.dtors)
		*(.dtors.*)
		*(.fini_array)
		*(SORT(.fini_array.*))
		___dtors_en = .;

		. = ALIGN(16);
		___smp_st = .;
		*(.smp.entry)
		*(.smp.text)
		. = ALIGN(64);
		*(.smp.data)
		___smp_en = .;
		
		/* sector align end */
		. = ALIGN(2048);
		___initialized_data_en = .;
	} = 0

	.bss : {
		___bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)

		. = ALIGN(16);
		. += 8;
		___bss_end = .;
	}

	___initial_stack_limit = .;
	. = ABSOLUTE(0xFFF0);
	___initial_stack = .;
	___heap_st = .;
	___end = .;

	/DISCARD/ : { *(.eh_frame) }
}
