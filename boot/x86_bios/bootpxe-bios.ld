ENTRY(bootpxe_early_entry)

MEMORY {
	lolo            : org = 0x0000000000001000, len = 0x000000006C00
	unreloc         : org = 0x0000000000007C00, len = 0x000000000200
	lohi    (rwxai) : org = 0x0000000000007E00, len = 0x000000018200
	bstk            : org = 0x0000000000020000, len = 0x000000010000
	bheap           : org = 0x0000000000030000, len = 0x000000070000
	hilo            : org = 0x0000000000100000, len = 0x0000FFEFFFFF
	hihi            : org = 0x0000000100000000, len = 0x7FFF00000000
	kern            : org = 0xFFFF800000000000, len = 0x800000000000
}

PHDRS {
	rxl PT_LOAD FLAGS(5);
	rwxl PT_LOAD FLAGS(7);
	rwl PT_LOAD FLAGS(6);
	nal PT_NULL FLAGS(0);
	rxh PT_LOAD FLAGS(5);
	rwh PT_LOAD FLAGS(6);
	roh PT_LOAD FLAGS(4);
}

SECTIONS {
	. = 0x1000;

	.bootpxetext16 : {
		___text_st = .;
		KEEP(*(.head))

		/* --- additional sectors --- */
		KEEP(*(.lowtext))

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
	} >lolo :rxl =0

	. = ALIGN(64);

	.bootpxesmp : {
		___smp_st = .;
		KEEP(*(.smp.entry))
		KEEP(*(.smp.text))
		. = ALIGN(64);
		KEEP(*(.smp.data))
		___smp_en = .;
	} >lolo :rwxl =0

	. = ALIGN(64);

	.bootpxedata16 : {
		bootinfo_st = .;
		KEEP(*(.bootinfo))

		KEEP(*(.lowdata))

		/* sector align end */
		. = ALIGN(512);
	} >lolo :rwl =0

	.bootpxeunusedlolo (NOLOAD) : {
		. = ABSOLUTE(0x7C00);
	} >lolo :nal =0

	. = 0x7C00;

	.unreloc (NOLOAD) : {
		___unreloc_st = .;
		. = 0x200;
	} >unreloc :nal =0

	. = 0x7E00;

	.bootpxetext : {
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		___text_en = .;

		*(.unreloc)

		. = ALIGN(4K);
	} >lohi :rxh =0

	. = ALIGN(64);

	.bootpxedata : {
		___data_st = .;
		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)
		___data_en = .;
	}

	. = ALIGN(64);

	.eh_frame (NOLOAD) : {
		*(.eh_frame)
	} >lohi :roh

	. = ALIGN(64);

	.bootpxebss : {
		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)
		___bss_en = .;
	} >lohi :rwh =0

	. = ALIGN(64);

	___bootpxe_image_en = .;

	/* Enforce that the image is under 124KB */
	/* If the following line errors, the bootloader is too big */
	. = 0x1F000;

	. = 0x20000;
	___initial_stack_limit = .;
	. = 0x30000;
	___initial_stack = .;
	. += 16;
	___heap_st = .;
	___end = .;

	. = ABSOLUTE(0x20000);
	___initial_stack_limit = .;
	. = ABSOLUTE(0x30000);
	___initial_stack = .;
	___heap_st = .;
	___end = .;

	. = .;
	___other_stuff_end = .;

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
