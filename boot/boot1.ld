ENTRY(entry)
SECTIONS {
	. = 0x7C00;

	.text : {
		*(.head)

		/* oem name */
		. = 3;
		mbr_oem_name = .;

		/* bios parameter block */
		. = 0xB;
		mbr_bpb = .;

		/* FAT32 extended bios parameter block */
		. = 0x24;
		mbr_ext_bpb = .;

		/* End of FAT32 extended bios parameter block */
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

		/* Error if it does not fit below 0x1C0 */
		. = 0x1C0;

		. = 0x1FE;
		SHORT(0x55AA);

		. = 512;
	} = 0

	/* Like .bss, but not guaranteed to be zero initialized */
	.uninit : {
		*(.uninit)
		___end = .;
	}

	. = ALIGN(2);
	___image_end = .;

	.debug           : { *(.debug)         }
	.debug_info		 : { *(.debug_info)    }
	.debug_abbrev	 : { *(.debug_abbrev)  }
	.debug_aranges	 : { *(.debug_aranges) }
	.debug_ranges	 : { *(.debug_ranges)  }
	.debug_frame	 : { *(.debug_frame)   }
	.debug_macro	 : { *(.debug_macro)   }
	.debug_line		 : { *(.debug_line)    }
	.debug_str		 : { *(.debug_str)     }
	.debug_loc		 : { *(.debug_loc)     }
	/*.comment        0 : { *(.comment)       }*/

	/DISCARD/       : { *(.comment)       }
}
