ENTRY(entry)
SECTIONS {
	.text 0xF000 : AT(0xF000) {
		*(.head)
		. = 0x5A;
		*(.code)
		*(.text)
		*(.data)
		*(.bss)
		*(.rel*)
		PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got*)
		*(.igot*)
		*(.iplt*)

		. = 0x1FE;
		SHORT(0x55AA);

		. = 512;
	} = 0

	/* Like .bss, but not guaranteed to be zero initialized */
	.uninit : {
		*(.uninit)
		___end = .;
	}

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
	/*.comment        0 : { *(.comment)       }*/

	/DISCARD/       : { *(.comment)       }
}
