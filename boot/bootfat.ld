ENTRY(entry)
OUTPUT_FORMAT(elf32-i386)

PHDRS {
	rwx	PT_LOAD FLAGS(7);
}

SECTIONS {
	. = 0x1000;

	.text : {
		___text_st = .;
		KEEP(*(.head))
		KEEP(*(.early))

		/* --- additional sectors --- */
		KEEP(*(.low))
		KEEP(*(.lowdata))

		. = ALIGN(4);
		PROVIDE(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		PROVIDE(__init_array_end = .);

		PROVIDE(__init_array_early_start = .);
		KEEP(*(.ctors))
		KEEP(*(.init_array))
		PROVIDE(__init_array_early_end = .);

		___dtors_st = .;
		KEEP(*(.dtors))
		KEEP(*(.dtors.*))
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		___dtors_en = .;

		/* Place 64 bit code high enough to be outside first 4KB */
		*(.text.code64)

		___smp_st = .;
		KEEP(*(.smp.entry))
		KEEP(*(.smp.text))
		. = ALIGN(64);
		KEEP(*(.smp.data))
		___smp_en = .;

		bootinfo_st = .;
		KEEP(*(.bootinfo))

		/* sector align end */
		. = ALIGN(512);
		___initialized_data_en = .;

		*(.text)
		*(.text.*)
		*(.text.unlikely)

		___text_en = .;

		___data_st = .;
		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)
		___data_en = .;

		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)
		___bss_en = .;
	} :rwx =0

	/* Max 124KB image */
	. = 0x1F000;

	. = 0x20000;
	___initial_stack_limit = .;
	. = 0x30000;
	___initial_stack = .;
	. += 16;
	___heap_st = .;
	___end = .;

/*
	.eh_frame       0 : { *(.eh_frame)      }
	.debug          0 : { *(.debug)         }
	.debug_info		0 : { *(.debug_info)    }
	.debug_abbrev	0 : { *(.debug_abbrev)  }
	.debug_aranges	0 : { *(.debug_aranges) }
	.debug_ranges	0 : { *(.debug_ranges)  }
	.debug_frame	0 : { *(.debug_frame)   }
	.debug_macro	0 : { *(.debug_macro)   }
	.debug_line		0 : { *(.debug_line)    }
	.debug_str		0 : { *(.debug_str)     }
	.debug_loc		0 : { *(.debug_loc)     }
	.comment        0 : { *(.comment)       }

	/DISCARD/		0 : { *(.interp)               }
	/DISCARD/		0 : { *(.comment)              }
	/DISCARD/		0 : { *(.note.gnu.build-id)    }
*/
}
