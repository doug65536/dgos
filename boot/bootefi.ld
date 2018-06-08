ENTRY(efi_main)

SECTIONS {
	. = SIZEOF_HEADERS;
	. = ALIGN(__section_alignment__);
	.text __image_base__ +
			(__section_alignment__ < 0x1000
			? .
			: __section_alignment__ ) : {
		__initialized_data_start = .;
		*(.text)
		*(.text.*)
		*(.text.unlikely)

		__ctors_start = .;
		*(.ctors)
		*(.ctors.*)
		*(.init_array)
		*(SORT(.init_array.*))
		__ctors_end = .;

		/* Place 64 bit code high enough to be outside first 4KB */
		*(.text.code64)

		*(.rodata)
		*(.rodata.*)
		*(.comment)

		. = ALIGN(4K);
		__initialized_data_end = .;
	}

	.got : ALIGN(4K) {
		PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
		*(.got)
	}

	.data ALIGN(4K) : {
		*(.data)
		*(.data.*)
	}

	.bss : {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(COMMON.*)
		*(COMMON*)

		. = ALIGN(16);
		__heap = .;
		. += 8;
		__bss_end = .;
	}

	/DISCARD/ : { *(.eh_frame) }
}
