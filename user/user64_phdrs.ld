ENTRY(_start)

PHDRS {
	read PT_LOAD FLAGS(4);
	tls PT_TLS FLAGS(0);
	exec PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
	/*hdr PT_PHDR FILEHDR PHDRS FLAGS(4);*/
}

SECTIONS {
	. = 0x400000;
	. += SIZEOF_HEADERS;

	PROVIDE_HIDDEN(__image_start = .);

	.rodata ALIGN(16) : {
		PROVIDE_HIDDEN(__rodata_st = .);
		*(.rodata)
		*(.rodata.*)
		PROVIDE_HIDDEN(__rodata_en = .);
	} /*:hdr*/ :read

	.tm_clone_table : {
		*(.tm_clone_table)
	} :read

	.eh_frame_hdr ALIGN(64) : {
		*(.eh_frame_hdr)
	} :read

	.eh_frame : {
		*(.eh_frame)
	} :read

	.rela.got ALIGN(8) : {
		*(.rela.got)
	} :read

	.rela.dyn : {
		*(.rela.dyn)
	} :read

	.rela.text : {
		*(.rela.text)
	} :read

	.gcc_except_table : {
		*(.gcc_except_table .gcc_except_table.*)
	} :read

	.gnu_extab : {
		*(.gnu_extab*)
	} :read

	/* Align for transition from r-- to r-x permission */
	.text ALIGN(4096) : {
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

	/* Align for transition from r-x to rw- permission */
	.data ALIGN(4096) : {
		PROVIDE_HIDDEN(___data_st = .);
		*(.data)
		*(.data.*)
		PROVIDE_HIDDEN(___data_en = .);
	} :data

	/* 64-bit align */
	.init_array ALIGN(8) : {
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
		PROVIDE_HIDDEN(__init_array_end = .);

		PROVIDE_HIDDEN(__init_array_early_start = .);
		KEEP(*(.init_array))
		KEEP(*(.ctors))
		PROVIDE_HIDDEN(__init_array_early_end = .);
	} :data

	/* 64-bit align */
	.fini_array ALIGN(8) : {
		PROVIDE_HIDDEN(__fini_array_start = .);
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fnit_array.*)))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} :data

	.got ALIGN(8) : {
		_GLOBAL_OFFSET_TABLE_ = .;
		*(.got)
	} :data

	.got.plt : {
		*(.got.plt)
	} :data

	.igot.plt : {
		*(.igot.plt)
	} :data

	.tdata ALIGN(16) : {
		PROVIDE_HIDDEN(___tdata_st = .);
		*(.tdata .tdata.* .gnu.linkonce.td.*)
		PROVIDE_HIDDEN(___tdata_en = .);
	} :data :tls

	.tbss ALIGN(8) (NOLOAD) : {
		PROVIDE_HIDDEN(___tbss_st = .);
		*(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon)
		. = ALIGN(8);
		PROVIDE_HIDDEN(___tbss_en = .);
	} :data :tls

	.bss (NOLOAD) : {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.bss)
		*(.bss.*)
		PROVIDE_HIDDEN(___bss_en = .);
	} :data

	. = ALIGN(4K);

	PROVIDE_HIDDEN(__image_end = .);

	.debug          0 : { *(.debug)         } :NONE
	.debug_info		0 : { *(.debug_info)    } :NONE
	.debug_abbrev	0 : { *(.debug_abbrev)  } :NONE
	.debug_aranges	0 : { *(.debug_aranges) } :NONE
	.debug_ranges	0 : { *(.debug_ranges)  } :NONE
	.debug_frame	0 : { *(.debug_frame)   } :NONE
	.debug_macro	0 : { *(.debug_macro)   } :NONE
	.debug_line		0 : { *(.debug_line)    } :NONE
	.debug_str		0 : { *(.debug_str)     } :NONE
	.debug_loc		0 : { *(.debug_loc)     } :NONE
	.comment        0 : { *(.comment)       } :NONE

	/DISCARD/       : { *(.comment)       }
}
