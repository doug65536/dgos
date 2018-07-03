ENTRY(entry)
SECTIONS {
	.text 0x600 : AT(0x600) {
		*(.text)
		*(.data)
		*(.bss)

		. = 0x1BE;
		partition_table = .;
		*(.parttab)

		. = 512;
	} = 0


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
	
	/DISCARD/		: { *(.interp)               }
	/DISCARD/		: { *(.comment)              }
	/DISCARD/		: { *(.note.gnu.build-id)    }
}
