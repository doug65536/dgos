ENTRY(entry)

PHDRS {
	text PT_LOAD FLAGS(5);
	readonly PT_LOAD FLAGS(4);
	data PT_LOAD FLAGS(6);
	tls PT_LOAD FLAGS(6);
}

SECTIONS {
	. = 0xFFFFFFFF80000000;

	PROVIDE_HIDDEN(___text_st = .);

	/* Code and read-only data */
	.text : AT (0x100000) ALIGN(4096) {
		*(.entry)
		*(.text)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.*))
		*(.text.unlikely)
	} :text

	.rodata : ALIGN(4096) {
		PROVIDE_HIDDEN(___init_st = .);
		*(.init_array)
		*(SORT(.init_array.*))
		PROVIDE_HIDDEN(___init_en = .);

		. = ALIGN(16);
		PROVIDE_HIDDEN(___fini_st = .);
		*(.fini_array)
		*(SORT(.fini_array.*))
		PROVIDE_HIDDEN(___fini_en = .);

		. = ALIGN(16);
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

		/* Provide the TLS size as a global variable */
		PROVIDE_HIDDEN(___tls_size = .);
		QUAD(___tbss_en - ___tdata_st);
		PROVIDE_HIDDEN(___tls_init_size = .);
		QUAD(___tdata_en - ___tdata_st);

		/* Provide the TLS address as a global variable */
		PROVIDE_HIDDEN(___tls_init_data_ptr = .);
		QUAD(___tdata_st);

		/* Provide the initial TLS address
		   as a global variable */
		PROVIDE_HIDDEN(___tls_main_tls_bottom_ptr = .);
		QUAD(___main_tls_bottom);

		/* Provide the address of the main TEB
		   as a global variable */
		PROVIDE_HIDDEN(___main_teb_ptr = .);
		QUAD(___main_teb);

		PROVIDE_HIDDEN(___text_en = .);
	} :readonly

	.eh_frame_hdr :  ALIGN(4096) {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} :readonly

	.eh_frame : ALIGN(4096) {
		KEEP(*(.eh_frame))
		*(.eh_frame.*)
	} :readonly

	.hash : ALIGN(4096) {
		PROVIDE_HIDDEN(___hash_st = .);
		*(.hash);
		PROVIDE_HIDDEN(___hash_en = .);
	} :readonly

	.dynsym : ALIGN(4096) {
		PROVIDE_HIDDEN(___dynsym_st = .);
		*(.dynsym);
		PROVIDE_HIDDEN(___dynsym_en = .);
	} :readonly

	.dynstr : ALIGN(4096) {
		PROVIDE_HIDDEN(___dynstr_st = .);
		*(.dynstr);
		PROVIDE_HIDDEN(___dynstr_en = .);
		. = ALIGN(4096);
	} :readonly

	.got : ALIGN(4096) {
		PROVIDE_HIDDEN(___got = .);
		*(.got*)
		*(.got.plt)
	} :readonly

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
	} :readonly

	.rela.plt : {
		PROVIDE_HIDDEN(___rela_plt = .);
		*(.rela.plt)
		PROVIDE_HIDDEN (__rela_iplt_start = .);
		*(.rela.iplt)
		PROVIDE_HIDDEN (__rela_iplt_end = .);
	} :readonly

	.plt : {
		PROVIDE_HIDDEN(___plt = .);
		*(.plt)
		*(.iplt)
	} :readonly

	.plt.got : {
		PROVIDE_HIDDEN(___plt_got = .);
		*(.plt.got)
	} :readonly

	.plt.bnd : {
		PROVIDE_HIDDEN(___plt_bnd = .);
		*(.plt.bnd)
	} :readonly

	.dynamic : ALIGN(4096) {
		PROVIDE_HIDDEN(___dynamic_st = .);
		*(.dynamic);
		PROVIDE_HIDDEN(___dynamic_en = .);
		. = ALIGN(4096);
	} :data

	/* Initialized data */
	.data  : ALIGN(4096) {
		*(.data)
		*(SORT(.data.*))

		/* Statically allocate room for the main */
		/* kernel thread TLS */
		. = ALIGN(16);
		PROVIDE_HIDDEN(___main_tls_bottom = .);
		. += ___tbss_en - ___tdata_st;
		PROVIDE_HIDDEN(___main_teb = .);
		/* 8 is sizeof thread_env_t! */
		QUAD(___main_teb);
		PROVIDE_HIDDEN(___main_teb_end = .);
		. = ALIGN(16);
	} :data

	.got : ALIGN(4096) {
		PROVIDE_HIDDEN(___got = .);
		*(.got*)
		*(.got.plt)
	} :data

	.bss : ALIGN(4096) {
		PROVIDE_HIDDEN(___bss_st = .);
		*(.bss)
		*(SORT(.bss.*))
		*(COMMON)
		. = ALIGN(16);
		PROVIDE_HIDDEN(___bss_en = .);
	} :data

	.tdata : ALIGN(4096) {
		PROVIDE_HIDDEN(___tdata_st = .);
		*(.tdata)
		. = ALIGN(16);
		PROVIDE_HIDDEN(___tdata_en = .);
	} :tls

	.tbss : ALIGN(4096) {
		PROVIDE_HIDDEN(___tbss_st = .);
		*(.tbss)
		*(.tcommon)
		. = ALIGN(16);
		PROVIDE_HIDDEN(___tbss_en = .);
	} :tls

	. = ALIGN(4096);
	PROVIDE_HIDDEN(___init_brk = .);

	/DISCARD/        : { *(.interp)        }
	.debug_info    0 : { *(.debug_info)    }
	.debug_abbrev  0 : { *(.debug_abbrev)  }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_macro   0 : { *(.debug_macro)   }
	.debug_line    0 : { *(.debug_line)    }
	.debug_str     0 : { *(.debug_str)     }
	.comment       0 : { *(.comment)       }
}
