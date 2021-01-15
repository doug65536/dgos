ENTRY(mbr_entry)

MEMORY {
	lolo    (rwxai) : org = 0x0000000000000500, len = 0x000000007700
	unreloc         : org = 0x0000000000007C00, len = 0x000000000200
	lohi            : org = 0x0000000000007E00, len = 0x000000018200
	bstk            : org = 0x0000000000020000, len = 0x000000010000
	bheap           : org = 0x0000000000030000, len = 0x000000070000
	hilo            : org = 0x0000000000100000, len = 0x0000FFEFFFFF
	hihi            : org = 0x0000000100000000, len = 0x7FFF00000000
	kern            : org = 0xFFFF800000000000, len = 0x800000000000
}

SECTIONS {
	. = 0x500;

	.mbrtext : {
		*(.mbrtext)
		. = ALIGN(64);
		*(.mbrdata)
		*(.mbrbss)

		. = .;

		. = 0x1BE;
		mbr_partition_table = .;
		*(.parttab)

		. = 512;
	} = 0

	. = 0x7C00;

	.mbrunreloc : {
		___mbr_unrelocated = .;
	} >unreloc

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

	/DISCARD/		: { *(.interp)               }
	/DISCARD/		: { *(.comment)              }
	/DISCARD/		: { *(.note.gnu.build-id)    }
}
