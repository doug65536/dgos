ENTRY(module_entry);
EXTERN(module_entry);

PHDRS {
	read PT_LOAD FILEHDR PHDRS FLAGS(4);
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);

	ehfr PT_GNU_EH_FRAME FLAGS(4);
	dyna PT_DYNAMIC FLAGS(4);
	driv 0x61000000 FLAGS(4);
	/*relr 0x6474e552 / *PT_GNU_RELRO* / FLAGS(6);*/
}

SECTIONS {
	/* Code and read-only data */

	. = 0;
	. += SIZEOF_HEADERS;

	.rodata ALIGN(8) : {
		PROVIDE_HIDDEN(___rodata_st = .);

		PROVIDE_HIDDEN(___callout_array_st = .);
		KEEP(*(SORT(.callout_array.*)))
		PROVIDE_HIDDEN(___callout_array_en = .);

		. = ALIGN(8);
		PROVIDE_HIDDEN(___rodata_fixup_insn_st = .);
		KEEP(*(.rodata.fixup.insn))
		PROVIDE_HIDDEN(___rodata_fixup_insn_en = .);

		PROVIDE_HIDDEN(___rodata_fixup_swapgs_st = .);
		KEEP(*(.rodata.fixup.swapgs))
		PROVIDE_HIDDEN(___rodata_fixup_swapgs_en = .);

		. = ALIGN(64);
		PROVIDE_HIDDEN(___rodata = .);
		*(.rodata)
		*(SORT(.rodata.*))

		*(.stub .gnu.linkonce.t.*)
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)

		PROVIDE_HIDDEN(___rodata_en = .);
	} :read

	.driver ALIGN(8) : {
		KEEP(*(.driver))
	} :read :driv

	.gnu.version ALIGN(8) : {
		*(.gnu.version)
	} :read

	.gnu.version_r ALIGN(8) : {
		*(.gnu.version_r)
	} :read

	.gnu.version_d ALIGN(8) : {
		*(.gnu.version_d)
	} :read

	.eh_frame ALIGN(8) : {
		PROVIDE_HIDDEN(___eh_frame_st = .);
		KEEP(*(.eh_frame))
		KEEP(*(.eh_frame.*))
		PROVIDE_HIDDEN(___eh_frame_en = .);
	} :read :ehfr

	.gcc_except_table ALIGN(8) : {
		*(.gcc_except_table .gcc_except_table.*)
	} :read

	.hash ALIGN(8) : {
		PROVIDE_HIDDEN(___hash_st = .);
		*(.hash);
		PROVIDE_HIDDEN(___hash_en = .);
	} :read

	.dynsym ALIGN(8) : {
		PROVIDE_HIDDEN(___dynsym_st = .);
		*(.dynsym);
		PROVIDE_HIDDEN(___dynsym_en = .);
	} :read

	.dynstr ALIGN(8) : {
		PROVIDE_HIDDEN(___dynstr_st = .);
		*(.dynstr);
		PROVIDE_HIDDEN(___dynstr_en = .);
	} :read

	.rela.plt ALIGN(8) : {
		PROVIDE_HIDDEN(___rela_plt = .);
		*(.rela.plt)
		PROVIDE_HIDDEN (__rela_iplt_start = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_end = .);
	} :read

	.rela.dyn ALIGN(8) : {
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

	.plt.got ALIGN(8) : {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	} :read

	.plt.bnd ALIGN(8) : {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	} :read

	. = ALIGN(4K);

	/*
	 * text must be the first executable section to correctly infer
	 * the symbol address when auto-loading kernel symbols
	 */

	.text ALIGN(4K) : {
		PROVIDE_HIDDEN(___text_st = .);
		*(.entry)
		*(.text)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.*))
		*(.text.unlikely)
		PROVIDE_HIDDEN(___text_en = .);
	} :text

	.plt ALIGN(8) : {
		PROVIDE_HIDDEN(___plt_st = .);
		*(.plt)
		*(.iplt)
		PROVIDE_HIDDEN(___plt_en = .);
	} :text

	. = ALIGN(4K);

	.got.plt ALIGN(8) : {
		*(.got.plt)
	} :data

	.igot.plt ALIGN(8) : {
		*(.igot.plt)
	} :data

	.got ALIGN(8) : {
		PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got)
	} :data

	.dynamic ALIGN(8) : {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	} :dyna :data

	.init_array ALIGN(8) : {
		PROVIDE_HIDDEN(__init_array_start = .);
		*(SORT_BY_INIT_PRIORITY(.ctors.*))
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		PROVIDE_HIDDEN(__init_array_end = .);

		PROVIDE_HIDDEN(__init_array_early_start = .);
		KEEP(*(.ctors))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN(__init_array_early_end = .);
	} :data

	.fini_array ALIGN(8) : {
		PROVIDE_HIDDEN(___fini_st = .);
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		PROVIDE_HIDDEN(___fini_en = .);
	} :data

	/* . = ALIGN(4K); */

	/* Initialized data */
	.data : {
		*(.data)
		*(SORT(.data.*))
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
	.debug          : { *(.debug)         }
	.debug_info		: { *(.debug_info)    }
	.debug_abbrev	: { *(.debug_abbrev)  }
	.debug_aranges	: { *(.debug_aranges) }
	.debug_ranges	: { *(.debug_ranges)  }
	.debug_frame	: { *(.debug_frame)   }
	.debug_macro	: { *(.debug_macro)   }
	.debug_line		: { *(.debug_line)    }
	.debug_str		: { *(.debug_str)     }
	.debug_loc		: { *(.debug_loc)     }
	.comment        : { *(.comment)       }
*/
}
