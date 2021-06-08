$(info x86_64 architecture specifics)

KERNEL_ARCH_SOURCES_SHARED = \
	kernel/arch/x86_64/entry.S \
	kernel/arch/x86_64/user_mem.cc \
	kernel/arch/x86_64/bios_data.cc \
	kernel/arch/x86_64/bios_data.h \
	kernel/arch/x86_64/cpu/isr.S \
	kernel/arch/x86_64/cpu/except_asm.S \
	kernel/arch/x86_64/cpu/syscall.S \
	kernel/arch/x86_64/cpu/nofault.cc \
	kernel/arch/x86_64/cpu/nofault.h \
	kernel/arch/x86_64/cpu/phys_alloc.cc \
	kernel/arch/x86_64/cpu/phys_alloc.h \
	kernel/arch/x86_64/cpu/apic.cc \
	kernel/arch/x86_64/cpu/apic.h \
	kernel/arch/x86_64/cpu/ioapic.cc \
	kernel/arch/x86_64/cpu/ioapic.h \
	kernel/arch/x86_64/cpu/mptables.h \
	kernel/arch/x86_64/cpu/asm_constants.h \
	kernel/arch/x86_64/cpu/cmos.cc \
	kernel/arch/x86_64/cpu/cmos.h \
	kernel/arch/x86_64/cpu/control_regs.cc \
	kernel/arch/x86_64/cpu/control_regs.h \
	kernel/arch/x86_64/cpu/cpu_broadcast.cc \
	kernel/arch/x86_64/cpu/cpu_broadcast.h \
	kernel/arch/x86_64/cpu/cpu.cc \
	kernel/arch/x86_64/cpu/cpuid.cc \
	kernel/arch/x86_64/cpu/cpuid.h \
	kernel/arch/x86_64/cpu/cpu_metrics.h \
	kernel/arch/x86_64/cpu/except_asm.h \
	kernel/arch/x86_64/cpu/except.cc \
	kernel/arch/x86_64/cpu/except.h \
	kernel/arch/x86_64/cpu/gdt.cc \
	kernel/arch/x86_64/cpu/gdt.h \
	kernel/arch/x86_64/cpu/idt.cc \
	kernel/arch/x86_64/cpu/idt.h \
	kernel/arch/x86_64/cpu/interrupts.cc \
	kernel/arch/x86_64/cpu/interrupts.h \
	kernel/arch/x86_64/cpu/perf.cc \
	kernel/arch/x86_64/cpu/perf.h \
	kernel/arch/x86_64/cpu/perf_reg.bits.h \
	kernel/arch/x86_64/cpu/ioport.cc \
	kernel/arch/x86_64/cpu/ioport.h \
	kernel/arch/x86_64/cpu/isr.h \
	kernel/arch/x86_64/cpu/legacy_pic.cc \
	kernel/arch/x86_64/cpu/legacy_pic.h \
	kernel/arch/x86_64/cpu/legacy_pit.cc \
	kernel/arch/x86_64/cpu/legacy_pit.h \
	kernel/arch/x86_64/cpu/math.cc \
	kernel/arch/x86_64/cpu/mmu.cc \
	kernel/arch/x86_64/cpu/nontemporal.cc \
	kernel/arch/x86_64/cpu/nontemporal.h \
	kernel/arch/x86_64/cpu/segrw.cc \
	kernel/arch/x86_64/cpu/segrw.h \
	kernel/arch/x86_64/cpu/spinlock_arch.h \
	kernel/arch/x86_64/cpu/syscall_dispatch.cc \
	kernel/arch/x86_64/cpu/syscall_dispatch.h \
	kernel/arch/x86_64/cpu/thread_impl.cc \
	kernel/arch/x86_64/cpu/thread_impl.h \
	kernel/arch/x86_64/elf64.cc \
	kernel/arch/x86_64/elf64_decl.h \
	kernel/arch/x86_64/elf64.h \
	kernel/arch/x86_64/gdbstub.cc \
	kernel/arch/x86_64/gdbstub.h \
	kernel/arch/x86_64/nano_time.cc \
	kernel/arch/x86_64/stacktrace.cc \
	kernel/arch/x86_64/stacktrace.h \
	kernel/arch/x86_64/types.h

bootefi_SOURCES += \
	boot/x86/retpoline.S \
	boot/x86/cpu.cc \
	boot/x86/arch_paging.cc \
	boot/x86/cpuid.cc \
	boot/x86/qemu_x86.cc

# mbr-elf

bin_PROGRAMS += mbr-elf
generate_symbols_list += mbr-elf

mbr_elf_SOURCES = \
	mbr/mbr.ld \
	mbr/mbr.S

mbr_elf_CXXFLAGS = \
	-DFROM_CXXFLAGS \
	-I$(top_srcdir)/boot \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(NO_LTO_FLAGS) \
	-static -DS00 \
	$(OPTIMIZE_SIZE_FLAGS) \
	-D__DGOS_BOOTLOADER__

mbr_elf_CFLAGS = \
	$(mbr_elf_CXXFLAGS)

mbr_elf_CCASFLAGS = \
	-DFROM_CCASFLAGS \
	-I$(top_srcdir)/boot \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)  \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) -static

mbr_elf_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(REGPARM_FLAGS) \
	$(ELF32_FLAGS) \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	$(NOSTDLIB_FLAGS) \
	$(LINKER_SEL) -static

EXTRA_mbr_elf_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_elf_LDADD = \
	$(LIBGCC_ELF32)

# mbr-bin

mbr-bin: mbr-elf
	$(OBJCOPY) -O binary --only-section=.mbrtext "$<" "$@"

# boot1-elf

bin_PROGRAMS += boot1-elf
generate_symbols_list += boot1-elf

boot1_elf_SOURCES = \
	boot/x86_bios/boot1.S \
	boot/x86_bios/boot1.ld

boot1_elf_CXXFLAGS = \
	-DFROM_CXXFLAGS \
	-I$(top_srcdir)/boot \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(NO_LTO_FLAGS) -static -DS01 \
	$(OPTIMIZE_SIZE_FLAGS) \
	-D__DGOS_BOOTLOADER__

boot1_elf_CFLAGS = $(boot1_elf_CXXFLAGS)

boot1_elf_CCASFLAGS = \
	-DFROM_CCASFLAGS \
	-I$(top_srcdir)/boot \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) -static

boot1_elf_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(REGPARM_FLAGS) \
	$(ELF32_FLAGS) \
	-Wl,-T,$(top_srcdir)/boot/x86_bios/boot1.ld \
	-Wl,-Map,boot1.map \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) -static -DS02 \
	$(NOSTDLIB_FLAGS)

EXTRA_boot1_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/x86_bios/boot1.ld

boot1_elf_LDADD = \
	$(LIBGCC_ELF32)

# boot1-bin

boot1-bin: boot1-elf
	$(OBJCOPY) -O binary --only-section=.boot1text "$<" "$@"

# libbootsect.a

noinst_LIBRARIES += libbootsect.a

libbootsect_a_SOURCES = \
	boot/x86_bios/bootcommon.S \
	boot/x86_bios/bioscall.S \
	boot/x86_bios/mbrpart.S \
	boot/x86_bios/cpu64_bios.S \
	boot/x86_bios/halt_bios.cc \
	boot/x86_bios/cpu_bios.cc \
	boot/x86_bios/screen_bios.cc \
	boot/x86_bios/physmem_bios.cc \
	boot/x86_bios/modelist_bios.cc \
	boot/x86_bios/tui_bios.cc \
	boot/x86_bios/boottable_bios.cc \
	boot/x86_bios/diskio.h \
	boot/x86_bios/diskio.cc \
	boot/x86_bios/serial_bios.cc \
	boot/x86/exception.S \
	boot/x86/gdt.S \
	boot/x86/retpoline.S \
	boot/x86/cpu.cc \
	boot/x86/cpuid.cc \
	boot/x86/mtrr.cc \
	boot/x86/mtrr.h \
	boot/x86/arch_paging.cc \
	boot/x86/qemu_x86.cc \
	boot/x86/cpu.h \
	boot/fs/fat32.cc \
	boot/fs/iso9660.cc \
	boot/fs/iso9660.h \
	boot/fs/fat32.h \
	boot/arch_paging.h \
	boot/ctors.cc \
	boot/assert.cc \
	boot/array_list.cc \
	boot/screen.cc \
	boot/serial.cc \
	boot/fs.cc \
	boot/malloc.cc \
	boot/string.cc \
	boot/string_char16.cc \
	boot/paging.cc \
	boot/elf64.cc \
	boot/rand.cc \
	boot/physmap.cc \
	boot/progressbar.cc \
	boot/messagebar.cc \
	boot/modelist.cc \
	boot/debug.cc \
	boot/likely.h \
	boot/mbrpart.h \
	boot/modeinfo.h \
	boot/progressbar.h \
	boot/messagebar.h \
	boot/paging.h \
	boot/rand.h \
	boot/types.h \
	boot/debug.h \
	boot/elf64.h \
	boot/elf64decl.h \
	boot/mpentry.h \
	boot/screen.h \
	boot/string.h \
	boot/physmem.h \
	boot/malloc.h \
	boot/fs.h \
	boot/vesa.h \
	boot/tui.h \
	boot/tui.cc \
	boot/bootmenu.cc \
	boot/include/boottable.h \
	boot/include/boottable_decl.h \
	boot/boottable.cc \
	boot/log2.h \
	boot/qemu.h \
	user/libutf/utf.cc

libbootsect_a_INCLUDES = \
	-I$(top_srcdir)/boot \
	-I$(top_srcdir)/boot/include \
	-I$(top_srcdir)/user/libutf

libbootsect_a_CXXFLAGS = \
	-DFROMCXXFLAGS \
	-I$(top_srcdir)/boot \
	$(NOSTDLIB_FLAGS) \
	$(COMPILER_FLAGS) \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(libbootsect_a_INCLUDES) \
	$(NO_PIC_FLAGS) -static -DS03 \
	$(FREESTANDING_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(NO_COMMON_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(NO_STACK_CHECK_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	-D__DGOS_BOOTLOADER__

#$(RETPOLINE_FLAGS)

libbootsect_a_CFLAGS =

libbootsect_a_CCASFLAGS = \
	-DFROMCCASFLAGS \
	-I$(top_srcdir)/boot \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(FREESTANDING_FLAGS) \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

# boot code

BOOT_SOURCES_SHARED = \
	boot/x86_bios/malloc_bios.cc \
	boot/x86_bios/mpentry_bios.S

# bootfat-elf

bin_PROGRAMS += bootfat-elf
generate_symbols_list += bootfat-elf

bootfat_elf_SOURCES = \
	boot/x86_bios/bootfat.S \
	$(BOOT_SOURCES_SHARED)

bootfat_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS) \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(NO_LTO_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) -static -DS04 \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

bootfat_elf_CCASFLAGS = \
	-DFROMCCASFLAGS \
	-I$(top_srcdir)/boot \
	$(bootfat_elf_CXXFLAGS)

# -N to turn off section alignment and make all sections rwx
bootfat_elf_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(LIBGCC_ELF32) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,--no-dynamic-linker \
	-Wl,-Map,bootfat.map \
	-Wl,-T,$(top_srcdir)/boot/x86_bios/bootfat.ld \
	-Wl,-N \
	$(LINKER_SEL) -static -DS05 \
	$(NOSTDLIB_FLAGS)

EXTRA_bootfat_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/x86_bios/bootfat.ld

bootfat_elf_LDADD = \
	libbootsect.a \
	$(LIBGCC_ELF32)

# bootfat-bin

bootfat-bin: bootfat-elf
	$(OBJCOPY) -O binary \
		--only-section=.bootfattext16 \
		--only-section=.bootfatrodata16 \
		--only-section=.bootfatdata16 \
		--only-section=.bootfatunusedlolo \
		--only-section=.bootfattext \
		--only-section=.bootfatrodata \
		--only-section=.bootfatdata \
		"$<" "$@"

# bootiso-elf

bin_PROGRAMS += bootiso-elf
generate_symbols_list += bootiso-elf

bootiso_elf_SOURCES = \
	boot/x86_bios/bootiso.S \
	$(BOOT_SOURCES_SHARED)

bootiso_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootiso_elf_CCASFLAGS = \
	-DFROM_CCASFLAGS \
	-I$(top_srcdir)/boot \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

bootiso_elf_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(ELF32_FLAGS) \
	$(LIBGCC_ELF32) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-T,$(top_srcdir)/boot/x86_bios/bootiso.ld \
	-Wl,-Map,bootiso-elf.map \
	-Wl,--no-dynamic-linker \
	$(LINKER_SEL) -static -DS06 \
	$(NOSTDLIB_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

EXTRA_bootiso_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/x86_bios/bootiso.ld

bootiso_elf_LDADD = \
	libbootsect.a \
	$(LIBGCC_ELF32)

# bootiso-bin

bootiso-bin: bootiso-elf
	$(OBJCOPY) -O binary -S \
		"$<" "$@"

#@--only-section=.head
#@--only-section=.bootisotext16
#@--only-section=.bootisotext
# bootpxe-bios

bin_PROGRAMS += bootpxe-bios-elf
generate_symbols_list += bootpxe-bios-elf

bootpxe_bios_elf_SOURCES = \
	boot/x86_bios/bootpxe-bios.ld \
	boot/x86_bios/mpentry_bios.S \
	boot/x86_bios/bootpxe.S \
	boot/x86_bios/pxe.h \
	boot/x86_bios/pxemain.cc \
	boot/x86_bios/pxemain.h \
	boot/x86_bios/pxemain_bios.cc \
	boot/x86_bios/pxemain_bios.h \
	boot/x86_bios/malloc_bios.cc

bootpxe_bios_elf_CXXFLAGS = \
	$(bootiso_elf_CXXFLAGS)

bootpxe_bios_elf_CCASFLAGS = \
	$(bootiso_elf_CCASFLAGS)

bootpxe_bios_elf_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(ELF32_FLAGS) \
	$(LIBGCC_ELF32) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-T,$(top_srcdir)/boot/x86_bios/bootpxe-bios.ld \
	-Wl,-Map,bootpxe-bios.map \
	$(LINKER_SEL) -static -DS07 \
	$(NOSTDLIB_FLAGS) \
	$(COMPILER_FLAGS)

bootpxe_bios_elf_LDADD = \
	libbootsect.a \
	$(LIBGCC_ELF32)

EXTRA_bootpxe_bios_elf_DEPENDENCIES = \
	boot/x86_bios/bootpxe-bios.ld

bootpxe-bios.map: bootpxe-bios-elf

# bootpxe-bios-bin

# extract binary image, strip debug
bootpxe-bios-bin: bootpxe-bios-elf Makefile.am
	$(OBJCOPY) -O binary -g \
		"$<" "$@"

BITFIELD_FILES += \
	$(top_srcdir)/kernel/arch/x86_64/cpu/apic.bits \
	$(top_srcdir)/kernel/arch/x86_64/cpu/pic.bits \
	$(top_srcdir)/kernel/arch/x86_64/cpu/perf_reg.bits

BOOT_FILES += \
	mbr-elf \
	boot1-elf \
	bootfat-elf \
	bootiso-elf \
	bootiso-bin \
	bootpxe-bios-elf
