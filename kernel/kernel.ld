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
		PROVIDE(___text_st = .);
		*(.entry)

		. = ALIGN(64);
		*(.text)
		*(.text.*)

		. = ALIGN(64);
		*(.text.unlikely)

		. = ALIGN(64);
		___init_st = .;
		*(.init_array)
		*(.init_array.*)
		___init_en = .;

		___fini_st = .;
		*(.fini_array)
		*(.fini_array.*)
		___fini_en = .;

		. = ALIGN(64);
		*(.eh_frame)

		. = ALIGN(64);
		*(.rodata)
		*(.rodata.*)
		PROVIDE(___text_en = .);
	} = 0xCC

	/* Initialized data */
	.data  : ALIGN(4096) {
		*(.data)
		*(.data.*)

		/* Statically allocate room for the main */
		/* kernel thread TLS */
		. = ALIGN(16);
		___main_tls_bottom = .;
		. += ___tbss_en - ___tdata_st /*SIZEOF(.tdata) + SIZEOF(.tbss)*/;
		___main_teb = .;
		/* 8 is sizeof thread_env_t! */
		QUAD(___main_teb);
		___main_teb_end = .;
		. = ALIGN(16);
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

	. = ALIGN(4096);

	/* Copy on write initialized thread_local */
	.tdata : {
		___tdata_st = .;
		*(.tdata)
		. = ALIGN(16);
		___tdata_en = .;
	}

	/* Zero page pool initialized thread_local */
	.tbss : {
		___tbss_st = .;
		*(.tbss)
		. = ALIGN(16);
		___tbss_en = .;
	}

	. = ALIGN(4096);
	___init_brk = .;

	.debug_info    0 : { *(.debug_info)    }
	.debug_abbrev  0 : { *(.debug_abbrev)  }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_macro   0 : { *(.debug_macro)   }
	.debug_line    0 : { *(.debug_line)    }
	.debug_str     0 : { *(.debug_str)     }
	.comment       0 : { *(.comment)       }
}
