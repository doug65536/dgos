ENTRY(entry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
	.text 0x1000 : AT(0x1000) {
		___text_st = .;
		*(.head)
		*(.early)

		/* --- additional sectors --- */
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		. = ALIGN(4);
		PROVIDE(__init_array_start = .);
		*(.ctors)
		*(.ctors.*)
		*(.init_array)
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		PROVIDE(__init_array_end = .);

		___dtors_st = .;
		*(.dtors)
		*(.dtors.*)
		*(.fini_array)
		*(SORT_BY_INIT_PRIORITY(.fini_array.*))
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

		bootinfo_st = .;
		*(.bootinfo)

		/* sector align end */
		. = ALIGN(512);
		___initialized_data_en = .;
	} = 0

	.bss : {
		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)
		___bss_en = .;
	}

	___initial_stack_limit = .;
	. = ABSOLUTE(0xFFF0);
	___initial_stack = .;
	. += 16;
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
