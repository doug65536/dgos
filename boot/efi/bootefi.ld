ENTRY(bootefi_entry)

MEMORY {
	ram (rwxai) : org = 0x400000, len = 0x400000
}

SECTIONS {
	. = ABSOLUTE(0x400000);
	
	.victim : { . += 16; }

	.text : {
		__text_st = .;

		KEEP(*(.entry))
		*(.text)
		*(.text.*)
		*(.text.unlikely)
	} >ram AT >ram

	. = ALIGN(64);

	/DISCARD/ : { *(.eh_frame); }
	/DISCARD/ : { *(.eh_frame_hdr); }

	. = ALIGN(64);

	.rela.plt : {
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

	. = ALIGN(64);

	.plt.got : {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	}

	. = ALIGN(64);

	.plt.bnd : {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	}

	. = ALIGN(64);

	.plt : {
		PROVIDE_HIDDEN(___plt = .);
		*(.plt)
		*(.iplt)
	}

	. = ALIGN(16K);

	.edata : {
		*(.edata)
	}

	.rodata : {
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

	. = ALIGN(16K);

	.reloc : { *(.reloc) }

	. = ALIGN(64);

	.hash : {
		PROVIDE_HIDDEN(___hash_st = .);
		*(.hash);
		PROVIDE_HIDDEN(___hash_en = .);
	}

	. = ALIGN(64);

	.dynsym : {
		PROVIDE_HIDDEN(___dynsym_st = .);
		*(.dynsym);
		*(.dynsym*);
		PROVIDE_HIDDEN(___dynsym_en = .);
	}

	. = ALIGN(64);

	.dynstr : {
		PROVIDE_HIDDEN(___dynstr_st = .);
		*(.dynstr);
		*(.dynstr*);
		PROVIDE_HIDDEN(___dynstr_en = .);
	}

	. = ALIGN(64);

	.got : {
		PROVIDE_HIDDEN(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got.plt)
		*(.got)
	}

	. = ALIGN(64);

	.rela.dyn : {
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

	. = ALIGN(64);

	.dynamic : {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
	}

	. = ALIGN(16K);

	.smp : {
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

	. = ALIGN(16k);

	.got : {
		PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got.plt)
		*(.got*)
	}

	. = ALIGN(64);

	.data : {
		*(.data)
		*(.data.*)
	}

	. = ALIGN(64);

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

	. = ALIGN(64);

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
