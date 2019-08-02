ENTRY(_start)

/*
PHDRS {
	hdr PT_PHDR FLAGS(0);
	read PT_LOAD FLAGS(4);
	tls PT_TLS FLAGS(0);
	exec PT_LOAD FLAGS(5);
	writ PT_LOAD FLAGS(6);

	ehfr PT_GNU_EH_FRAME FLAGS(4);
	dyna PT_DYNAMIC FLAGS(4);
}
*/

SECTIONS {
	PROVIDE_HIDDEN(__image_start = .);

	/* read only */

	. = 0x400000;
	. += SIZEOF_HEADERS;

	.rodata : {
		PROVIDE_HIDDEN(__rodata_start = .);
		*(.rodata)
		*(.rodata.*)
		PROVIDE_HIDDEN(__rodata_end = .);
	} /*:read :hdr*/

	.tm_clone_table : {
		*(.tm_clone_table)
	} /*:read*/

	.eh_frame_hdr : {
		*(.eh_frame_hdr)
	} /*:read*/

	.eh_frame : {
		*(.eh_frame)
	} /*:read*/

	.rela.got : {
		*(.rela.got)
	} /*:read*/

	.rela.dyn : {
		PROVIDE_HIDDEN(___rela_dyn_st = .);
		*(.rela.init)
		*(.rela.init_array)
		*(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
		*(.rela.fini)
		*(.rela.fini_array)
		*(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
		*(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
		*(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
		*(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
		*(.rela.ctors)
		*(.rela.dtors)
		*(.rela.got)
		*(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
		*(.rela.ldata .rela.ldata.* .rela.gnu.linkonce.l.*)
		*(.rela.lbss .rela.lbss.* .rela.gnu.linkonce.lb.*)
		*(.rela.lrodata .rela.lrodata.* .rela.gnu.linkonce.lr.*)
		*(.rela.ifunc)
		*(.rela.*)
		PROVIDE_HIDDEN (__rela_dyn_en = .);
	} /*:read*/

	.plt.got : {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	} /*:read*/

	.plt.bnd : {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	} /*:read*/

	. = ALIGN(4K);

	/*
	 * text must be the first executable section to correctly infer
	 * the symbol address when auto-loading kernel symbols
	 */

	.text : {
		PROVIDE_HIDDEN(___text_st = .);
		*(.entry)
		*(.text)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.*))
		*(.text.unlikely)
		PROVIDE_HIDDEN(___text_en = .);
	} /*:exec*/

	.plt : {
		PROVIDE_HIDDEN(___plt_st = .);
		*(.plt)
		*(.iplt)
		PROVIDE_HIDDEN(___plt_en = .);
	} /*:exec*/

	.rela.iplt : {
		PROVIDE_HIDDEN(__rela_iplt_start = .);
		*(.rela.iplt)
		*(.rela.iplt.*)
		PROVIDE_HIDDEN(__rela_iplt_end = .);
	}

	. = ALIGN(4K);

	.got : {
		PROVIDE_HIDDEN(___got = .);
		*(.got)
	} /*:writ*/

	.got.plt : {
		*(.got.plt)
	} /*:writ*/

	.dynamic : {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	} /*:dyna :writ*/

	.init_array : {
		PROVIDE_HIDDEN(__init_array_start = .);
		*(.init_array)
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		PROVIDE_HIDDEN(__init_array_end = .);
	} /*:writ*/

	.fini_array : {
		PROVIDE_HIDDEN(__fini_array_start = .);
		*(.fini_array)
		*(SORT_BY_INIT_PRIORITY(.fini_array.*))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} /*:writ*/

	/* . = ALIGN(4K); */

	/* Initialized data */
	.data : {
		*(.data)
		*(SORT(.data.*))
	} /*:writ*/

	.tdata : {
		PROVIDE_HIDDEN(___tdata_start = .);
		*(.tdata .tdata.* .gnu.linkonce.td.*)
		PROVIDE_HIDDEN(___tdata_end = .);
	} /*:read :tls*/

	.tbss (NOLOAD) : {
		PROVIDE_HIDDEN(___tbss_start = .);
		*(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon)
		PROVIDE_HIDDEN(___tbss_end = .);
	} /*:tls*/

	.bss : {
		PROVIDE_HIDDEN(___bss_start = .);
		*(.bss)
		*(.dynbss)
		*(SORT(.bss.*))
		*(COMMON)
		. = ALIGN(16);
		PROVIDE_HIDDEN(___bss_end = .);
	} /*:writ*/

	. = ALIGN(4K);
	PROVIDE_HIDDEN(___image_end = .);

	.debug          0 : { *(.debug)         } /*:NONE*/
	.debug_info		0 : { *(.debug_info)    } /*:NONE*/
	.debug_abbrev	0 : { *(.debug_abbrev)  } /*:NONE*/
	.debug_aranges	0 : { *(.debug_aranges) } /*:NONE*/
	.debug_ranges	0 : { *(.debug_ranges)  } /*:NONE*/
	.debug_frame	0 : { *(.debug_frame)   } /*:NONE*/
	.debug_macro	0 : { *(.debug_macro)   } /*:NONE*/
	.debug_line		0 : { *(.debug_line)    } /*:NONE*/
	.debug_str		0 : { *(.debug_str)     } /*:NONE*/
	.debug_loc		0 : { *(.debug_loc)     } /*:NONE*/
	.comment        0 : { *(.comment)       } /*:NONE*/
}
