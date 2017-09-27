.code16
.section .head
.globl entry
entry:
	jmp entry_start

.org entry+8
.globl bootinfo_primary_volume_desc
bootinfo_primary_volume_desc:
.space 4

.globl bootinfo_file_location
bootinfo_file_location:
.space 4

.globl bootinfo_file_length
bootinfo_file_length:
.space 4

.globl bootinfo_checksum
bootinfo_checksum:
.space 4

.globl bootinfo_reserved
bootinfo_reserved:
.space 10*4

# The kernel finds the boot code by reading this vector
# MP entry point vector at offset 64
.globl mp_entry_vector
mp_entry_vector:
.int 0

# The kernel finds the boot device information by reading this vector
# Int 13h, AH=48h Get drive parameters
# Vector at offset 68
.globl boot_device_info_vector
boot_device_info_vector:
.int 0

# The kernel finds the VBE information by reading this vector
# Vector at offset 72
.globl vbe_info_vector
vbe_info_vector:
.int 0

.section .early
entry_start:
	ljmp $0,$zero_cs

zero_cs:
