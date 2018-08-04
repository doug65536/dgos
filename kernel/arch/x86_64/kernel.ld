ENTRY(entry)

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

	/* Code and read-only data */
	.text 0xFFFFFFFF80000000 : AT (0x100000) {
		PROVIDE_HIDDEN(___text_st = .);
		/* ISR code is hot and should be page aligned because it is cloned */
		PROVIDE_HIDDEN(___isr_st = .);
		*(.text.isr)
		PROVIDE_HIDDEN(___isr_en = .);

		/* cache line align start of hot code */
		. = ALIGN(64);
		*(.text.hot .text.hot.*)
		*(.text)
		*(SORT(.text.*))
		*(.text.unlikely)

		/* startup code away from normal code to maximize locality */
		*(.entry)
		*(.text.startup .text.startup.*)
	} :text

	.rodata : ALIGN(64) {
		PROVIDE_HIDDEN(___ctors_st = .);
		*(.init_array)
		*(SORT_BY_INIT_PRIORITY(.init_array.*))
		*(.ctors)
		*(.ctors.*)
		PROVIDE_HIDDEN(___ctors_en = .);

		. = ALIGN(16);
		PROVIDE_HIDDEN(___dtors_st = .);
		*(.fini_array)
		*(SORT_BY_INIT_PRIORITY(.fini_array.*))
		*(.dtors)
		*(.dtors.*)
		PROVIDE_HIDDEN(___dtors_en = .);

		. = ALIGN(32);

		PROVIDE_HIDDEN(___callout_array_st = .);
		*(SORT(.callout_array.*))
		PROVIDE_HIDDEN(___callout_array_en = .);

		. = ALIGN(64);
		PROVIDE_HIDDEN(___rodata = .);
		*(.rodata)
		*(SORT(.rodata.*))

		*(.stub .text.* .gnu.linkonce.t.*)
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)

		/* Expose addresses/sizes to kernel as initialized data */

		/* Provide the highest address taken by kernel as
		   a global variable */
		. = ALIGN(8);
		PROVIDE_HIDDEN(___top_physaddr = .);
		QUAD((___init_brk - ___text_st) + 0x100000);

		PROVIDE_HIDDEN(___text_en = .);
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

	.got : ALIGN(64) {
		PROVIDE_HIDDEN(___got = .);
		*(.got)
		*(.got.plt)
	} :readonly

	.rela : ALIGN(64) {
		PROVIDE_HIDDEN(___rela_dyn = .);
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
		*(.rela*)
		*(.rel*)
	} :readonly

	.rela.plt : ALIGN(64) {
		PROVIDE_HIDDEN(___rela_entry_start = .);
		*(.rela.entry)
		PROVIDE_HIDDEN(___rela_entry_end = .);

		PROVIDE_HIDDEN(___rela_plt_start = .);
		*(.rela.plt)
		PROVIDE_HIDDEN(___rela_plt_end = .);

		PROVIDE_HIDDEN (__rela_iplt_start = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_end = .);
	} :readonly

	.plt.got : ALIGN(64) {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	} :readonly

	.plt.bnd : ALIGN(64) {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	} :readonly

	.plt : ALIGN(64) {
		PROVIDE_HIDDEN(___plt = .);
		*(.plt)
		*(.iplt)
	} :readonly

	.dynamic : ALIGN(4096) {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	} :data

	.got : ALIGN(64) {
		PROVIDE_HIDDEN(___got = .);
		*(.got*)
		*(.got.plt)
	} :data

	/* Initialized data */
	.data : ALIGN(4096) {
		PROVIDE_HIDDEN(___data_st = .);
		*(.data)
		*(SORT(.data.*))
		PROVIDE_HIDDEN(___data_en = .);
	} :data

	.bss (NOLOAD) : {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.bss)
		*(SORT(.bss.*))
		*(.dynbss)
		*(COMMON)
		PROVIDE_HIDDEN(___bss_en = .);
	} :data

	.bspguard (NOLOAD) : ALIGN(4096) {
		. += 4096;
	} :guard

	.bspstk (NOLOAD) : ALIGN(4096) {
		/* emergency stack for early bootstrap */
		. = . + 65536;
		/* bootstrap CPU stack */
		*(.bspstk)
	} :stacks

	.note : {
		*(.note*)
	}

	. = ALIGN(4096);
	PROVIDE_HIDDEN(___init_brk = .);

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

	/DISCARD/		0 : { *(.interp)               }
	/DISCARD/		0 : { *(.gnu*)                 }
	/DISCARD/		0 : { *(.comment)              }
	/DISCARD/		0 : { *(.note.gnu.build-id)    }
}
