ENTRY(bootefi_entry)

SECTIONS {
	/DISCARD/ : { *(.eh_frame); }
	/DISCARD/ : { *(.eh_frame_hdr); }
	/DISCARD/       : { *(.comment)       }

	. = __ImageBase;
	
	.broken : { . += 16; } =0

	.text : {
		__text_st = .;

		KEEP(*(.entry))
		*(.text)
		*(SORT(.text.*))
		*(.text.unlikely)
	}
	
	. = ALIGN(16K);

	.edata : {
		*(.edata)
	}

	.rodata : {
		. = ALIGN(16);
		__init_array_start = .;
		KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		__init_array_end = .;

		__init_array_early_start = .;
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.ctors)))
		__init_array_early_end = .;

		. = ALIGN(16);
		___dtors_st = .;
		KEEP(*(SORT_BY_INIT_PRIORITY(.dtors.*)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.dtors)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		___dtors_en = .;

		/* Place 64 bit code high enough to be outside first 4KB */
		*(.text.code64)

		*(.rodata)
		*(.rodata.*)
	}

	. = ALIGN(16K);

	.reloc : { *(.reloc) }

	. = ALIGN(16K);

	.smp : {
		/* Gather all trampoline-critical code and data into this section */
		/* It will protect data from being lost when exiting boot services */
		/* This section is copied to a 4KB aligned heap block */
		___smp_st = ABSOLUTE(.);
		KEEP(*(.smp.entry))
		KEEP(*(.smp.text))
		. = ALIGN(64);
		KEEP(*(.smp.data))
		___smp_en = ABSOLUTE(.);
	}

	. = ALIGN(16k);

	.data : {
		*(.data)
		*(.data.*)
	}

	. = ALIGN(64);

	.bss : {
		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)

		. = ALIGN(16);
		__heap = .;
		. += 8;
		___bss_en = .;
	}

	. = ALIGN(64);

/*	.debug          (NOLOAD) : { *(.debug)         }
	.debug_info		(NOLOAD) : { *(.debug_info)    }
	.debug_abbrev	(NOLOAD) : { *(.debug_abbrev)  }
	.debug_aranges	(NOLOAD) : { *(.debug_aranges) }
	.debug_ranges	(NOLOAD) : { *(.debug_ranges)  }
	.debug_frame	(NOLOAD) : { *(.debug_frame)   }
	.debug_macro	(NOLOAD) : { *(.debug_macro)   }
	.debug_line		(NOLOAD) : { *(.debug_line)    }
	.debug_str		(NOLOAD) : { *(.debug_str)     }
	.debug_loc		(NOLOAD) : { *(.debug_loc)     }*/

}
