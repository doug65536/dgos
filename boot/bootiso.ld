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
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		___ctors_en = .;

		. = ALIGN(16);
		___dtors_st = .;
		*(.dtors)
		*(.dtors.*)
		*(.fini_array)
		*(SORT_BY_INIT_PRIORITY(.fini_array.*))
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
		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)

		. = ALIGN(16);
		. += 8;
		___bss_en = .;
	}

	___initial_stack_limit = .;
	. = ABSOLUTE(0xFFF0);
	___initial_stack = .;
	___heap_st = .;
	___end = .;

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
	.got            0 : { *(.got*)          }
	.rel            0 : { *(.rel*)          }
	.iplt           0 : { *(.iplt*)         }
	.igot           0 : { *(.igot*)         }

	/DISCARD/		0 : { *(.interp)               }
	/DISCARD/		0 : { *(.comment)              }
	/DISCARD/		0 : { *(.note.gnu.build-id)    }
}
