ENTRY(bootiso_entry)

MEMORY {
	/* 23KB region */
	lolo    (rwxai) : org = 0x0000000000001000, len = 0x000000006C00
	/* el torito boot is never unrelocated */
	lohi            : org = 0x0000000000007C00, len = 0x000000018400
	bstk            : org = 0x0000000000020000, len = 0x000000010000
	bheap           : org = 0x0000000000030000, len = 0x000000070000
	hilo            : org = 0x0000000000100000, len = 0x0000FFEFFFFF
	hihi            : org = 0x0000000100000000, len = 0x7FFF00000000
	kern            : org = 0xFFFF800000000000, len = 0x800000000000
}

/* NOTE: only the first 23KB is loaded by the BIOS, */
/* in case a dumbass BIOS has its stack in the 1st 64KB (seabios), */
/* the startup code loads the rest */
/* The BIOS loads it straight to segment 0x100 (address 0x1000) */

SECTIONS {
	. = 0x1000;

	.head . : {
		___text_st = .;
		KEEP(*(.head))
	} >lolo =0

	.bootisotext16 . : {
		KEEP(*(.lowtext))
	} >lolo =0

	.bootisodata16 . : {
		KEEP(*(.lowdata))
		KEEP(*(.lowbss))

		/* Align so the low stuff ends on a big CD sector size boundary */
		. = ALIGN(2K);
	} >lolo =0

	.bootisounused16 : {
		. = ABSOLUTE(0x7c00);
	} >lolo =0

	.bootisotext . : {
		. = ALIGN(4K);
		*(.text)
		*(.text.*)
		*(.text.unlikely)
		. = ALIGN(64);
	} >lohi =0

	.eh_frame : {
		*(.eh_frame)
	} >lohi =0

	.bootisodata : {
		*(.rodata)
		*(.rodata.*)

		. = ALIGN(16);
		PROVIDE(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		PROVIDE(__init_array_end = .);

		PROVIDE(__init_array_early_start = .);
		KEEP(*(.ctors))
		KEEP(*(.init_array))
		PROVIDE(__init_array_early_end = .);

		. = ALIGN(16);
		___dtors_st = .;
		KEEP(*(.dtors))
		KEEP(*(.dtors.*))
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		___dtors_en = .;

		. = ALIGN(16);
		___smp_st = .;
		KEEP(*(.smp.entry))
		KEEP(*(.smp.text))
		. = ALIGN(64);
		KEEP(*(.smp.data))
		___smp_en = .;
		*(.data)
		*(.data.*)

		/* sector align end */
		. = ALIGN(2K);
		___initialized_data_en = .;

		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)

		. = .;

		. = ALIGN(16);
		. += 8;
		___bss_en = .;

		/* Sector align bin file size */
		. = ALIGN(2K);
	} >lohi =0

	. = .;

	. = ABSOLUTE(0x20000);
	___initial_stack_limit = .;
	. = ABSOLUTE(0x30000);
	___initial_stack = .;
	___heap_st = .;
	___end = .;

	/DISCARD/         : { *(.unreloc) }

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
