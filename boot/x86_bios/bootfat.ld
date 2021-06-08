ENTRY(bootfat_entry)
OUTPUT_FORMAT(elf32-i386)

MEMORY {
	lolo            : org = 0x0000000000001000, len = 0x000000006C00
	lohi    (rwxai) : org = 0x0000000000007C00, len = 0x000000018400
	bstk            : org = 0x0000000000020000, len = 0x000000010000
	bheap           : org = 0x0000000000030000, len = 0x000000070000
	hilo            : org = 0x0000000000100000, len = 0x0000FFEFFFFF
	hihi            : org = 0x0000000100000000, len = 0x7FFF00000000
	kern            : org = 0xFFFF800000000000, len = 0x800000000000
}

PHDRS {
	rwx	PT_LOAD FLAGS(7);
}

SECTIONS {
	. = 0x1000;

	.bootfattext16 : {
		___text_st = .;
		KEEP(*(.head))

		/* --- additional sectors --- */
		KEEP(*(.lowtext))
	} >lolo :rwx =0

	.bootfatrodata16 : {
		/* Place 64 bit code high enough to be outside first 4KB */
		*(.text.code64)
	} >lolo :rwx =0

	.bootfatdata16 : {
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
	} >lolo :rwx =0

	.bootfatunusedlolo : {
		. = ABSOLUTE(0x7C00);
	} >lolo =0

	. = 0x7C00;

	.bootfattext : {
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		___text_en = .;

		. = ALIGN(64);
	} >lohi :rwx =0

	.bootfatrodata : {
		___rodata_st = .;
		*(.rodata)
		*(.rodata.*)
		___rodata_en = .;
	}

	.bootfatdata : {
		___data_st = .;
		*(.data)
		*(.data.*)
		___data_en = .;

		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)
		___bss_en = .;

		*(.unreloc)
	} >lohi :rwx =0

	/* Max 124KB image */
	. = 0x1F000;

	. = ABSOLUTE(0x20000);
	___initial_stack_limit = .;
	. = ABSOLUTE(0x30000);
	___initial_stack = .;
	. += 16;
	___heap_st = .;
	___end = .;

	/DISCARD/ : { *(.eh_frame)      }

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
