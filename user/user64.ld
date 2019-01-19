ENTRY(_start)

PHDRS {
	/* 4 = read */
	/* 5 = 4+1 = read+execute */
	/* 6 = 4+2 = read+write */
	exec PT_LOAD FLAGS(5);
	read PT_LOAD FLAGS(4);
	writ PT_LOAD FLAGS(6);
}

SECTIONS {
	. = 0x400000 + SIZEOF_HEADERS;

	PROVIDE_HIDDEN(__image_start = .);

	.text : {
		PROVIDE_HIDDEN(__text_start = .);
		*(.text)
		*(.text.*)
		PROVIDE_HIDDEN(__text_end = .);
	} :exec

	.rela.plt : {
		PROVIDE_HIDDEN(__plt_start = .);
		*(.plt)
		PROVIDE_HIDDEN(__plt_end = .);
		PROVIDE_HIDDEN(__iplt_start = .);
		*(.iplt)
		PROVIDE_HIDDEN(__iplt_end = .);
		PROVIDE_HIDDEN(__rela_iplt_start = .);
		*(.rela.iplt)
		*(.rela.iplt.*)
		PROVIDE_HIDDEN(__rela_iplt_end = .);
	} :exec

	/* Align for transition from r-x to r-- permission */
	. = ALIGN(4K);

	.rodata : {
		PROVIDE_HIDDEN(__rodata_start = .);
		*(.rodata)
		*(.rodata.*)
		PROVIDE_HIDDEN(__rodata_end = .);
	} :read

	/* 64-bit align */
	. = ALIGN(8);

	.init_array : {
		PROVIDE_HIDDEN(__init_array_start = .);
		*(.init_array)
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		PROVIDE_HIDDEN(__init_array_end = .);
	} :read

	/* 64-bit align */
	. = ALIGN(8);

	.fini_array : {
		PROVIDE_HIDDEN(__fini_array_start = .);
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fnit_array.*)))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} :read

	.gcc_except_table   : { *(.gcc_except_table .gcc_except_table.*) }
	.gnu_extab   : { *(.gnu_extab*) }

	.tdata : {
		PROVIDE_HIDDEN(___tdata_st = .);
		*(.tdata)
		*(.tdata.*)
		PROVIDE_HIDDEN(___tdata_en = .);
	} :read

	.tbss (NOLOAD) : {
		PROVIDE_HIDDEN(___tbss_st = .);
		*(.tbss)
		*(.tbss.*)
		PROVIDE_HIDDEN(___tbss_en = .);
	} :read

	/* Align for transition from r-x to rw- permission */
	. = ALIGN(4K);

	.data : {
		PROVIDE_HIDDEN(___data_st = .);
		*(.data)
		*(.data.*)
		PROVIDE_HIDDEN(___data_en = .);
	} :writ

	.bss (NOLOAD) : {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.bss)
		*(.bss.*)
		PROVIDE_HIDDEN(___bss_en = .);
	} :writ

	PROVIDE_HIDDEN(__image_end = .);

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
}
