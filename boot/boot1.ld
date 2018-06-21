ENTRY(entry)
SECTIONS {
	.text 0xF000 : AT(0xF000) {
		*(.head)
		. = 0x5A;
		*(.code)
		
		. = 0x1FE;
		SHORT(0x55AA);

		. = 512;
	} = 0
	
	/* Like .bss, but not guaranteed to be zero initialized */
	.uninit : {
		*(.uninit)
		___end = .;
	}
}
