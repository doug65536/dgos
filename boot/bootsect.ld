ENTRY(entry)
SECTIONS {
    .text 0x7C00 : {
        __initialized_data_start = .;
        *(.head)
        *(.head.*)
        bootsect.o(.text.startup)
        bootsect.o(.text)
        bootsect.o(.text.*)
        bootsect.o(.text.unlikely)

		. = ALIGN(4);
		bootsect.o(.data)
        bootsect.o(.data.*)
        bootsect.o(.rodata)
        bootsect.o(.rodata.*)

        . = 0x1BE;
        partition_table = .;
        *(.parttab)

        /* --- additional sectors --- */
        *(.text)
        *(.text.*)
        *(.text.unlikely)

        . = ALIGN(16);

        *(.data)
        *(.data.*)
        *(.rodata)
        *(.rodata.*)

		/* sector align end */
        . = ALIGN(512);
		__initialized_data_end = .;
	} = 0xCC

    .bss : {
        ___bss_st = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
		___bss_en = .;

		. = ALIGN(16);
	}

	__initial_stack = 0xfff0;
	__heap = 0x500;
	__heap_end = 0x7bf0;
}
