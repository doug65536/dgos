ENTRY(entry)

SECTIONS {
	/*
	 * Total address space is 256TB
	 * 64TB of address space is reserved for user mode programs
	 * 64TB is reserved for the kernel
	 * This sidesteps any issues with dealing with canonical addresses
	 * By 2024 we should have about 1TB of ram
	 * By 2033 we might have a problem with 64TB limit
	 */

	/* Code and read-only data */
	.text 0x0000400000000000 : {
		*(.text)
		*(.text.*)

		. = ALIGN(64);
		*(.text.unlikely)

		. = ALIGN(64);
		*(.init_array)
		*(.fini_array)

		. = ALIGN(64);
		*(.eh_frame)

		. = ALIGN(64);
		*(.rodata)
		*(.rodata.*)
	} = 0xCC

	/* Initialized data */
	.data  : ALIGN(4096) {
		*(.data)
		*(.data.*)
	} = 0x00

	/* Zero page pool initialized */
	.bss : ALIGN(4096) {
		PROVIDE(___bss_st = .);
		*(.bss)
		*(.bss.*)
		*(COMMON)
		PROVIDE(___bss_en = .);
	}

	.got.plt : ALIGN(4096) {
		*(.got.plt)
	}

	/* Copy on write initialized thread_local */
	.tdata : ALIGN(4096) {
		*(.tdata)
		. = ALIGN(4096);
	}

	/* Zero page pool initialized thread_local */
	.tbss : {
		PROVIDE(___tbss_st = .);
		*(.tbss)
		PROVIDE(___tbss_en = .);
	}

	.debug 0 : {
		*(.debug_info   )
		*(.debug_abbrev )
		*(.debug_aranges)
		*(.debug_macro  )
		*(.debug_line   )
		*(.debug_str    )
		*(.comment      )
	}
}
