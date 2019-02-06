ENTRY(module_entry)

PHDRS {
	read PT_LOAD FILEHDR PHDRS FLAGS(4);
	ehfr PT_GNU_EH_FRAME FLAGS(4);
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
	dyna PT_DYNAMIC FLAGS(4);
}

SECTIONS {
	/* Code and read-only data */

	. = 0;
	. += SIZEOF_HEADERS;

	.init_array : {
		PROVIDE_HIDDEN(___init_st = .);
		*(.init_array)
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		PROVIDE_HIDDEN(___init_en = .);
	} :read

	.fini_array : {
		PROVIDE_HIDDEN(___fini_st = .);
		*(.fini_array)
		*(SORT_BY_INIT_PRIORITY(.fini_array.*))
		PROVIDE_HIDDEN(___fini_en = .);
	} :read

	.rodata : {
		PROVIDE_HIDDEN(___rodata_st = .);

		PROVIDE_HIDDEN(___callout_array_st = .);
		*(SORT(.callout_array.*))
		PROVIDE_HIDDEN(___callout_array_en = .);

		. = ALIGN(64);
		PROVIDE_HIDDEN(___rodata = .);
		*(.rodata)
		*(SORT(.rodata.*))

		*(.stub .gnu.linkonce.t.*)
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)

		PROVIDE_HIDDEN(___rodata_en = .);
	} :read

	.eh_frame_hdr : {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} :read

	.eh_frame : {
		KEEP(*(.eh_frame))
		*(.eh_frame.*)
	} :read :ehfr

	.hash : {
		PROVIDE_HIDDEN(___hash_st = .);
		*(.hash);
		PROVIDE_HIDDEN(___hash_en = .);
	} :read

	.dynsym : {
		PROVIDE_HIDDEN(___dynsym_st = .);
		*(.dynsym);
		PROVIDE_HIDDEN(___dynsym_en = .);
	} :read

	.dynstr : {
		PROVIDE_HIDDEN(___dynstr_st = .);
		*(.dynstr);
		PROVIDE_HIDDEN(___dynstr_en = .);
	} :read

	.rela.plt : {
		PROVIDE_HIDDEN(___rela_plt = .);
		*(.rela.plt)
		PROVIDE_HIDDEN (__rela_iplt_start = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_end = .);
	} :read

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
	} :read

	.plt.got : {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	} :read

	.plt.bnd : {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	} :read

	.got : {
		PROVIDE_HIDDEN(___got = .);
		*(.got)
	} :read

	.got.plt : {
		*(.got.plt)
	} :read

	. = ALIGN(4K);

	.text : {
		PROVIDE_HIDDEN(___text_st = .);
		*(.entry)
		*(.text)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.*))
		*(.text.unlikely)
		PROVIDE_HIDDEN(___text_en = .);
	} :text

	. = ALIGN(16);

	.plt : {
		PROVIDE_HIDDEN(___plt_st = .);
		*(.plt)
		*(.iplt)
		PROVIDE_HIDDEN(___plt_en = .);
	} :text

	. = ALIGN(4K);

	.dynamic : {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	} :dyna :data

	/* Initialized data */
	.data  : {
		*(.data)
		*(SORT(.data.*))
		. = ALIGN(16);
	} :data

	.bss : {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.bss)
		*(.dynbss)
		*(SORT(.bss.*))
		*(COMMON)
		. = ALIGN(16);
		PROVIDE_HIDDEN(___bss_en = .);
	} :data

	. = ALIGN(4K);
	PROVIDE_HIDDEN(___image_en = .);

/*
	.debug_info		(NOLOAD) : { *(.debug_info)    }
	.debug_abbrev	(NOLOAD) : { *(.debug_abbrev)  }
	.debug_aranges	(NOLOAD) : { *(.debug_aranges) }
	.debug_ranges	(NOLOAD) : { *(.debug_ranges) }
	.debug_macro	(NOLOAD) : { *(.debug_macro)   }
	.debug_line		(NOLOAD) : { *(.debug_line)    }
	.debug_str		(NOLOAD) : { *(.debug_str)     }

	/DISCARD/       : { *(.interp)        }
	/DISCARD/		: { *(.comment)       }
	/DISCARD/		: { *(.gnu.version_d) }
	/DISCARD/		: { *(.gnu.version_r) }
	/DISCARD/		: { *(.gnu.version)   }
*/
}
