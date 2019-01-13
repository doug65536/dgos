ENTRY(_start)

PHDRS {
	/* 4 = read */
	/* 5 = 4+1 = read+execute */
	/* 6 = 4+2 = read+write */
	text PT_LOAD FLAGS(5);
	readonly PT_LOAD FLAGS(4);
	data PT_LOAD FLAGS(6);
	guard PT_LOAD FLAGS(0);
	stacks PT_LOAD FLAGS(6);
}

SECTIONS {
	PROVIDE_HIDDEN(__image_start = .);

	.text 0x400000 : {
		PROVIDE_HIDDEN(__text_start = .);
		*(.text)
		*(.text.*)
		PROVIDE_HIDDEN(__text_end = .);
	} :text

	.rela.plt ALIGN(16) : {
		PROVIDE_HIDDEN(__plt_start = .);
		*(.plt)
		PROVIDE_HIDDEN(__plt_end = .);
		PROVIDE_HIDDEN(__iplt_start = .);
		*(.iplt)
		PROVIDE_HIDDEN(__iplt_end = .);
		PROVIDE_HIDDEN(__rela_iplt_start = .);
		*(.rela.iplt)
		*(SORT(.rela.iplt.*))
		PROVIDE_HIDDEN(__rela_iplt_end = .);
	} :text

	.rodata ALIGN(4K) : {
		PROVIDE_HIDDEN(__rodata_start = .);
		*(.rodata)
		*(.rodata.*)
		PROVIDE_HIDDEN(__rodata_end = .);
	} :readonly

	.init_array ALIGN(8) : {
		PROVIDE_HIDDEN(__init_array_start = .);
		*(.init_array)
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		PROVIDE_HIDDEN(__init_array_end = .);
	} :readonly

	.fini_array ALIGN(8) : {
		PROVIDE_HIDDEN(__fini_array_start = .);
		*(.fini_array)
		*(SORT_BY_INIT_PRIORITY(.fnit_array.*))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} :readonly

	.eh_frame_hdr :  ALIGN(64) {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} :readonly

	.eh_frame : ALIGN(64) {
		PROVIDE_HIDDEN(___eh_frame_st = .);
		KEEP(*(.eh_frame))
		*(.eh_frame.*)
		PROVIDE_HIDDEN(___eh_frame_en = .);
		/* Must end with 4 zero bytes */
		LONG(0);
	} :readonly

	.tdata ALIGN(4K) : {
		PROVIDE_HIDDEN(___tdata_st = .);
		*(.tdata)
		*(.tdata.*)
		PROVIDE_HIDDEN(___tdata_en = .);
	} :readonly

	.tbss (NOLOAD) : {
		PROVIDE_HIDDEN(___tbss_st = .);
		*(.tbss)
		*(.tbss.*)
		PROVIDE_HIDDEN(___tbss_en = .);
	} :readonly

	.data ALIGN(4K) : {
		PROVIDE_HIDDEN(___data_st = .);
		*(.data)
		*(.data.*)
		PROVIDE_HIDDEN(___data_en = .);
	} :data

	.bss (NOLOAD) : {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.bss)
		*(.bss.*)
		PROVIDE_HIDDEN(___bss_en = .);
	} :data

	PROVIDE_HIDDEN(__image_end = .);

	.debug          0 : { *(.debug)         } :readonly
	.debug_info		0 : { *(.debug_info)    } :readonly
	.debug_abbrev	0 : { *(.debug_abbrev)  } :readonly
	.debug_aranges	0 : { *(.debug_aranges) } :readonly
	.debug_ranges	0 : { *(.debug_ranges)  } :readonly
	.debug_frame	0 : { *(.debug_frame)   } :readonly
	.debug_macro	0 : { *(.debug_macro)   } :readonly
	.debug_line		0 : { *(.debug_line)    } :readonly
	.debug_str		0 : { *(.debug_str)     } :readonly
	.debug_loc		0 : { *(.debug_loc)     } :readonly
	.comment        0 : { *(.comment)       } :readonly
}
