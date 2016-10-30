ENTRY(entry)

SECTIONS {
	/*
	 * Total address space is 256TB
	 * 64TB of address space is reserved for user mode programs
	 * 64TB is reserved for the kernel
	 * This sidesteps any issues with dealing with canonical addresses
	 * By 2024 we should have about 1TB of ram
	 * By 2033 we might have a problem with 64TB limit
	 */
	.text 0x0000400000000000 : {
		*(.text)
		*(.text.*)
		*(.text.unlikely)
	} = 0x90

	.data : ALIGN(4096) {
		*(.data)
		*(.data.*)
		*(.rodata)
		*(.rodata.*)
	} = 0x00

	.bss : ALIGN(4096) {
		___bss_st = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		___bss_en = .;
	}
}
