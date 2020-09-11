ENTRY(entry)

PHDRS {
	/* 4 = read */
	/* 5 = 4+1 = read+execute */
	/* 6 = 4+2 = read+write */

	/* r-- */
	readonly PT_LOAD FILEHDR PHDRS FLAGS(4);

	/* r-x */
	text PT_LOAD FLAGS(5);

	/* rw- */
	data PT_LOAD FLAGS(6);

	/* --- */
	guard PT_LOAD FLAGS(0);

	/* rw- */
	stacks PT_LOAD FLAGS(6);

	/* rw- */
	asan PT_LOAD FLAGS(6);
}

SECTIONS {
	. = 0xFFFFFFFF80000000;

	/* r-- begins */
	PROVIDE(__image_start = .);

	. += SIZEOF_HEADERS;

	.rodata : {
		PROVIDE_HIDDEN(___rodata_st = .);

		PROVIDE_HIDDEN(___callout_array_st = .);
		*(SORT(.callout_array.*))
		PROVIDE_HIDDEN(___callout_array_en = .);

		PROVIDE_HIDDEN(___rodata = .);
		*(.rodata)

		. = ALIGN(8);
		PROVIDE_HIDDEN(___rodata_fixup_insn_st = .);
		*(.rodata.fixup.insn)
		PROVIDE_HIDDEN(___rodata_fixup_insn_en = .);

		*(SORT(.rodata.*))

		*(.gcc_except_table*)

		. = ALIGN(8);
		PROVIDE_HIDDEN(__nofault_tab_st = .);
		*(.nofault.tab.st)
		PROVIDE_HIDDEN(__nofault_tab_en = .);
		*(.nofault.tab.en)
		PROVIDE_HIDDEN(__nofault_tab_lp = .);
		*(.nofault.tab.lp)
		PROVIDE_HIDDEN(__nofault_tab_xx = .);

		/* define and initialize a global variable named __nofault_tab_sz */
		PROVIDE_HIDDEN(__nofault_tab_sz = .);
		QUAD(__nofault_tab_en - __nofault_tab_st);
		ASSERT((__nofault_tab_lp - __nofault_tab_en) ==
			(__nofault_tab_en - __nofault_tab_st), "Messed up SOA");
		ASSERT((__nofault_tab_xx - __nofault_tab_lp) ==
			(__nofault_tab_en - __nofault_tab_st), "Messed up SOA");

		/* define and initialize a global variable named __nofault_text_sz */
		PROVIDE_HIDDEN(__nofault_text_sz = .);
		QUAD(__nofault_text_en - __nofault_text_st);

		*(.stub .gnu.linkonce.t.*)
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)

		*(.comment)

		PROVIDE_HIDDEN(___rodata_en = .);
	} :readonly

	. = ALIGN(64);

	.eh_frame_hdr : {
		KEEP(*(.eh_frame_hdr))
		KEEP(*(.eh_frame_entry .eh_frame_entry.*))
	} :readonly

	.eh_frame : {
		PROVIDE_HIDDEN(___eh_frame_st = .);
		KEEP(*(.eh_frame))
		KEEP(*(.eh_frame.*))
		PROVIDE_HIDDEN(___eh_frame_en = .);
		/* Must end with 4 zero bytes */
		LONG(0);
	} :readonly

	.hash : ALIGN(64) {
		PROVIDE_HIDDEN(___hash_st = .);
		*(.hash);
		PROVIDE_HIDDEN(___hash_en = .);
	} :readonly

	.dynsym : ALIGN(64) {
		PROVIDE_HIDDEN(___dynsym_st = .);
		*(.dynsym);
		*(.dynsym*);
		PROVIDE_HIDDEN(___dynsym_en = .);
	} :readonly

	.dynstr : ALIGN(64) {
		PROVIDE_HIDDEN(___dynstr_st = .);
		*(.dynstr);
		*(.dynstr*);
		PROVIDE_HIDDEN(___dynstr_en = .);
	} :readonly

	. = ALIGN(16);

	.rela.plt : {
		PROVIDE_HIDDEN(___rela_plt = .);
		*(.rela.plt)
		PROVIDE_HIDDEN (__rela_iplt_start = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_end = .);
	} :readonly

	.rela.dyn : ALIGN(64) {
		PROVIDE_HIDDEN(__rela_dyn_st = .);
		*(.rela.dyn)
		*(.rela.init)
		*(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
		*(.rela.fini)
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
		PROVIDE_HIDDEN(___rela_plt = .);
		*(.rela.plt)
		PROVIDE_HIDDEN (__rela_iplt_start = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_end = .);
		*(.rela.*)
		PROVIDE_HIDDEN (__rela_dyn_en = .);
	} :readonly
	PROVIDE_HIDDEN(__rela_dyn_en = .);

	.note : {
		*(.note*)
	} :readonly

	/* r-x begins */
	. = ALIGN(16K);

	.text : {
		PROVIDE_HIDDEN(___text_st = .);
		/* ISR code is hot and should be page aligned because it is cloned */
		PROVIDE_HIDDEN(___isr_st = .);
		*(SORT_BY_NAME(.text.isr.*))
		*(.text.isr)
		PROVIDE_HIDDEN(___isr_en = .);

		PROVIDE_HIDDEN(__nofault_text_st = .);
		*(.nofault.text)
		PROVIDE_HIDDEN(__nofault_text_en = .);

		/* cache line align start of hot code */
		. = ALIGN(64);
		*(.text.hot .text.hot.*)
		*(.text)
		*(SORT(.text.*))
		*(.text.unlikely)

		/* startup code away from normal code to maximize locality */
		*(.entry)
		*(.text.startup .text.startup.*)
		PROVIDE_HIDDEN(___text_en = .);
	} :text

	.plt : {
		PROVIDE_HIDDEN(___plt = .);
		*(.plt)
		*(.iplt)
	} :text

	/* rw- begins */
	. = ALIGN(16K);

	.cpus : {
		*(.data.cpus)
	} :data

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

	.fini_array ALIGN(8) : {
		PROVIDE_HIDDEN(___dtors_st = .);
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		KEEP(*(.dtors))
		KEEP(*(SORT_BY_INIT_PRIORITY(.dtors.*)))
		PROVIDE_HIDDEN(___dtors_en = .);
	} :data

	.dynamic : {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	} :data

	.plt.got : {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	} :data

	.plt.bnd : {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	} :data

	.got.plt : {
		*(.got.plt)
	} :data

	.got : {
		PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got)
	} :data

	/* Initialized data */
	.data : {
		PROVIDE_HIDDEN(___data_st = .);
		*(.data)
		*(SORT(.data.*))
		PROVIDE_HIDDEN(___data_en = .);
	} :data

	.bss (NOLOAD) : {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.instr)
		*(.bss)
		*(SORT(.bss.*))
		*(.dynbss)
		*(COMMON)
		PROVIDE_HIDDEN(___bss_en = .);
	} :data

	/* --- begins */
	. = ALIGN(16K);

	.bspguard (NOLOAD) : {
		. += 4K;
	} :guard

	/* rw- begins */
	. = ALIGN(16K);

	.bspstk (NOLOAD) : {
		PROVIDE_HIDDEN(__bspstk_st = .);
		/* emergency stack for early bootstrap */
		. = . + 65536;
		/* bootstrap CPU stack */
		KEEP(*(.bspstk))
		PROVIDE_HIDDEN(__bspstk_en = .);
	} :stacks

	/* Separate so it won't cause excessive asan pool pages to be allocated */
	.asanbss (NOLOAD) : {
		KEEP(*(.asanbss))
	} :asan

	. = ALIGN(16K);
	PROVIDE_HIDDEN(___init_brk = .);

/*
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
*/

/*
	/DISCARD/		0 : { *(.debug)         } :readonly
	/DISCARD/		0 : { *(.debug_info)    } :readonly
	/DISCARD/		0 : { *(.debug_abbrev)  } :readonly
	/DISCARD/		0 : { *(.debug_aranges) } :readonly
	/DISCARD/		0 : { *(.debug_ranges)  } :readonly
	/DISCARD/		0 : { *(.debug_frame)   } :readonly
	/DISCARD/		0 : { *(.debug_macro)   } :readonly
	/DISCARD/		0 : { *(.debug_line)    } :readonly
	/DISCARD/		0 : { *(.debug_str)     } :readonly
	/DISCARD/		0 : { *(.debug_loc)     } :readonly
	/DISCARD/		0 : { *(.comment)       } :readonly
*/

/*
	/DISCARD/		0 : { *(.interp)               }
	/DISCARD/		0 : { *(.gnu*)                 }
	/DISCARD/		0 : { *(.comment)              }
	/DISCARD/		0 : { *(.note.gnu.build-id)    }
*/
}
