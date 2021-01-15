ENTRY(boot1_entry)

MEMORY {
	lolo    (rwxai) : org = 0x0000000000000C00, len = 0x000000007000
	unreloc         : org = 0x0000000000007C00, len = 0x000000000200
	lohi            : org = 0x0000000000007E00, len = 0x000000018200
	bstk            : org = 0x0000000000020000, len = 0x000000010000
	bheap           : org = 0x0000000000030000, len = 0x000000070000
	hilo            : org = 0x0000000000100000, len = 0x0000FFEFFFFF
	hihi            : org = 0x0000000100000000, len = 0x7FFF00000000
	kern            : org = 0xFFFF800000000000, len = 0x800000000000
}

SECTIONS {
	. = 0x0C00;

	.boot1text . : {
		*(.head)

		/* oem name */
		. = 3;
		boot1_mbr_oem_name = .;

		/* bios parameter block */
		. = 0xB;
		boot1_mbr_bpb = .;

		/* FAT32 extended bios parameter block */
		. = 0x24;
		boot1_mbr_ext_bpb = .;

		/* End of FAT32 extended bios parameter block */
		. = 0x5A;
		*(.entry)
		*(.code)
		*(.text)
		*(.text.*)
		*(.data)
		*(.data.*)
		*(.bss)
		*(.bss.*)
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
	} >lolo =0

	.boot1unreloc (NOLOAD) : {
		___boot1_unrelocated = .;
	} >unreloc

	/* Like .bss, but not guaranteed to be zero initialized */
	.boot1uninit : {
		*(.uninit)
		___boot1_end = .;
	} >lolo

	. = ALIGN(2);
	___boot1_image_end = .;

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
