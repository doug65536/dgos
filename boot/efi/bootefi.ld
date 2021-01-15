ENTRY(bootefi_entry)

SECTIONS {
	. = SIZEOF_HEADERS;
	. = ALIGN(__section_alignment__);
	.text __image_base__ +
			(__section_alignment__ < 0x1000
			? .
			: __section_alignment__ ) : {
		__text_st = .;

		KEEP(*(.entry))
		*(.text)
		*(.text.*)
		*(.text.unlikely)
	}

	.rela.plt : ALIGN(64) {
		PROVIDE_HIDDEN(___rela_entry_st = .);
		*(.rela.entry)
		PROVIDE_HIDDEN(___rela_entry_en = .);

		PROVIDE_HIDDEN(___rela_plt_st = .);
		*(.rela.plt)
		PROVIDE_HIDDEN(___rela_plt_en = .);

		PROVIDE_HIDDEN (__rela_iplt_st = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_en = .);
	}

	.plt.got : ALIGN(64) {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	}

	.plt.bnd : ALIGN(64) {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	}

	.plt : ALIGN(64) {
		PROVIDE_HIDDEN(___plt = .);
		*(.plt)
		*(.iplt)
	}

	.edata ALIGN(4K) : {
		*(.edata)

		. = ALIGN(16);
		__init_array_start = .;
		KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		__init_array_end = .;

		__init_array_early_start = .;
		KEEP(*(.init_array))
		KEEP(*(.ctors))
		__init_array_early_end = .;

		. = ALIGN(16);
		___dtors_st = .;
		KEEP(*(SORT_BY_INIT_PRIORITY(.dtors.*)))
		KEEP(*(.dtors))
		KEEP(*(.fini_array))
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		___dtors_en = .;

		/* Place 64 bit code high enough to be outside first 4KB */
		*(.text.code64)

		*(.rodata)
		*(.rodata.*)
	}

	.smp ALIGN(4K) : {
		/* Gather all trampoline-critical code and data into this section */
		/* It will protect data from being lost when exiting boot services */
		/* This section is copied to a 4KB aligned heap block */
		___smp_st = ABSOLUTE(.);
		KEEP(*(.smp.entry))
		KEEP(*(.smp.text))
		. = ALIGN(64);
		KEEP(*(.smp.data))
		___smp_en = ABSOLUTE(.);
	}

	.reloc ALIGN(4K) : { *(.reloc) }

	.hash : ALIGN(64) {
		PROVIDE_HIDDEN(___hash_st = .);
		*(.hash);
		PROVIDE_HIDDEN(___hash_en = .);
	}

	.dynsym : ALIGN(64) {
		PROVIDE_HIDDEN(___dynsym_st = .);
		*(.dynsym);
		*(.dynsym*);
		PROVIDE_HIDDEN(___dynsym_en = .);
	}

	.dynstr : ALIGN(64) {
		PROVIDE_HIDDEN(___dynstr_st = .);
		*(.dynstr);
		*(.dynstr*);
		PROVIDE_HIDDEN(___dynstr_en = .);
	}

	.got : ALIGN(64) {
		PROVIDE_HIDDEN(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got.plt)
		*(.got)
	}

	.rela.dyn : ALIGN(64) {
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
	}

	.dynamic : ALIGN(4096) {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	}

	.got : ALIGN(64) {
		PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got.plt)
		*(.got*)
	}

	.data ALIGN(4K) : {
		*(.data)
		*(.data.*)
	}

	.bss : {
		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)

		. = ALIGN(16);
		__heap = .;
		. += 8;
		___bss_en = .;
	}

	/DISCARD/       : { *(.comment)       }
/*	.debug          (NOLOAD) : { *(.debug)         }
	.debug_info		(NOLOAD) : { *(.debug_info)    }
	.debug_abbrev	(NOLOAD) : { *(.debug_abbrev)  }
	.debug_aranges	(NOLOAD) : { *(.debug_aranges) }
	.debug_ranges	(NOLOAD) : { *(.debug_ranges)  }
	.debug_frame	(NOLOAD) : { *(.debug_frame)   }
	.debug_macro	(NOLOAD) : { *(.debug_macro)   }
	.debug_line		(NOLOAD) : { *(.debug_line)    }
	.debug_str		(NOLOAD) : { *(.debug_str)     }
	.debug_loc		(NOLOAD) : { *(.debug_loc)     }*/

}
