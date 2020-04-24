## Process this file with automake to produce Makefile.in

#SUFFIXES: .y
#
#%.c %.h %.out: %.y
#	lemon $<
#

noinst_PROGRAMS =

DUMPDIR = $(top_builddir)/dump

STARTUP_FILE_NAMES = \
	crt0 crt0pc80 crt0pc64 crt0pc32

STDLIB_FILE_NAMES = c g m stdc++

STARTUP_FILES_INSTALLED = \
	$(patsubst %,$(top_builddir)/sysroot/lib/%.o,$(STARTUP_FILE_NAMES))

STDLIB_STATIC_FILES_INSTALLED = \
	$(patsubst %,$(top_builddir)/sysroot/lib/lib%.a,$(STDLIB_FILE_NAMES))

STDLIB_SHARED_FILES_INSTALLED = \
	$(patsubst %,$(top_builddir)/sysroot/lib/lib%.so,$(STDLIB_FILE_NAMES))

ALL_STDLIB_INSTALLED = \
	$(STARTUP_FILES_INSTALLED) \
	$(STDLIB_STATIC_FILES_INSTALLED)

BUILT_SOURCES = \
	$(DUMPDIR)/qemu-debug-out \
	$(DUMPDIR)/call-trace-out \
	$(generated_headers) \
	$(ALL_STDLIB_INSTALLED)

BOOTLOADER_COMMON_FLAGS = \
	-D__DGOS__

#sysroot/lib/libc.so \
#sysroot/lib/libg.so \
#sysroot/lib/libm.so \
#sysroot/lib/libstdc++.so

#LINKER_DEBUG = -Wl,--orphan-handling,warn
LINKER_DEBUG =

STARTUPFILES =

noinst_LIBRARIES =
bin_PROGRAMS =
#bootefi-i386

generate_symbols_list =
generate_kallsym_list =

# mbr-elf

bin_PROGRAMS += mbr-elf
generate_symbols_list += mbr-elf

mbr_elf_SOURCES = \
	mbr/mbr.ld \
	mbr/mbr.S

mbr_elf_CXXFLAGS = \
	-DFROM_CXXFLAGS \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(NO_LTO_FLAGS) -static \
	$(OPTIMIZE_SIZE_FLAGS)

mbr_elf_CFLAGS =
	$(mbr_elf_CXXFLAGS)

mbr_elf_CCASFLAGS = \
	-DFROM_CCASFLAGS \
	$(COMPILER_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) -static

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

$(top_builddir)/mbr-bin: $(top_builddir)/mbr-elf
	$(OBJCOPY) -O binary --only-section=.text "$<" "$@"

# boot1-elf

bin_PROGRAMS += boot1-elf
generate_symbols_list += boot1-elf

boot1_elf_SOURCES = \
	boot/boot1.S \
	boot/boot1.ld

boot1_elf_CXXFLAGS = \
	-DFROM_CXXFLAGS \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(NO_LTO_FLAGS) -static \
	$(OPTIMIZE_SIZE_FLAGS)

boot1_elf_CFLAGS =

boot1_elf_CCASFLAGS = \
	-DFROM_CCASFLAGS \
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
	-Wl,-m,elf_i386 \
	-Wl,-T,$(top_srcdir)/boot/boot1.ld \
	-Wl,-N \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) -static \
	$(NOSTDLIB_FLAGS)

EXTRA_boot1_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/boot1.ld

boot1_elf_LDADD = \
	$(LIBGCC_ELF32)

# boot1-bin

$(top_builddir)/boot1-bin: $(top_builddir)/boot1-elf
	$(OBJCOPY) -O binary --only-section=.text "$<" "$@"

# libbootsect.a

noinst_LIBRARIES += libbootsect.a

libbootsect_a_SOURCES = \
	boot/bootcommon.S \
	boot/bioscall.S \
	boot/retpoline.S \
	boot/diskio.cc \
	boot/ctors.cc \
	boot/assert.cc \
	boot/halt_bios.cc \
	boot/gdt.S \
	boot/cpu_bios.cc \
	boot/bootinfo.S \
	boot/array_list.cc \
	boot/screen.cc \
	boot/screen_bios.cc \
	boot/fs.cc \
	boot/fat32.cc \
	boot/iso9660.cc \
	boot/cpu.cc \
	boot/cpuid.cc \
	boot/cpu64_bios.S \
	boot/malloc.cc \
	boot/string.cc \
	boot/string_char16.cc \
	boot/utf.cc \
	boot/paging.cc \
	boot/elf64.cc \
	boot/rand.cc \
	boot/physmap.cc \
	boot/physmem_bios.cc \
	boot/vesa.cc \
	boot/vesa_bios.cc \
	boot/progressbar.cc \
	boot/debug.cc \
	boot/part.s \
	boot/exception.s \
	boot/likely.h \
	boot/part.h \
	boot/vesainfo.h \
	boot/progressbar.h \
	boot/paging.h \
	boot/rand.h \
	boot/types.h \
	boot/iso9660.h \
	boot/debug.h \
	boot/cpu.h \
	boot/utf.h \
	boot/elf64.h \
	boot/exception.h \
	boot/elf64decl.h \
	boot/mpentry.h \
	boot/screen.h \
	boot/fat32.h \
	boot/string.h \
	boot/physmem.h \
	boot/diskio.h \
	boot/malloc.h \
	boot/fs.h \
	boot/vesa.h \
	boot/tui.h \
	boot/tui.cc \
	boot/tui_bios.cc \
	boot/bootmenu.cc \
	boot/include/boottable.h \
	boot/include/boottable_decl.h \
	boot/boottable.cc \
	boot/boottable_bios.cc \
	boot/log2.h \
	boot/qemu.cc \
	boot/qemu.h

libbootsect_a_INCLUDES = \
	-I$(top_srcdir)/boot \
	-I$(top_srcdir)/boot/include

libbootsect_a_CXXFLAGS = \
	-DFROMCXXFLAGS \
	$(NOSTDLIB_FLAGS) \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(libbootsect_a_INCLUDES) \
	$(NO_PIC_FLAGS) -static \
	$(FREESTANDING_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(NO_COMMON_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(NO_STACK_CHECK_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

#$(RETPOLINE_FLAGS)

libbootsect_a_CFLAGS =

libbootsect_a_CCASFLAGS = \
	-DFROMCCASFLAGS \
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
	boot/malloc_bios.cc \
	boot/mpentry_bios.S

# bootfat-elf

bin_PROGRAMS += bootfat-elf
generate_symbols_list += bootfat-elf

bootfat_elf_SOURCES = \
	boot/bootfat.S \
	$(BOOT_SOURCES_SHARED)

bootfat_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootfat_elf_CCASFLAGS = \
	-DFROMCCASFLAGS \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(ELF32_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) -static \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

# -N to turn off section alignment and make all sections rwx
bootfat_elf_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(LIBGCC_ELF32) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-Map,$(top_builddir)/bootfat.map \
	-Wl,-T,$(top_srcdir)/boot/bootfat.ld \
	-Wl,-N \
	$(LINKER_SEL) -static \
	$(NOSTDLIB_FLAGS)

EXTRA_bootfat_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/bootfat.ld

bootfat_elf_LDADD = \
	libbootsect.a \
	$(LIBGCC_ELF32)

# bootfat-bin

$(top_builddir)/bootfat-bin: $(top_builddir)/bootfat-elf
	$(OBJCOPY) -O binary --only-section=.text "$<" "$@"

# bootiso-elf

bin_PROGRAMS += bootiso-elf
generate_symbols_list += bootiso-elf

bootiso_elf_SOURCES = \
	boot/bootiso.S \
	$(BOOT_SOURCES_SHARED)

bootiso_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootiso_elf_CCASFLAGS = \
	-DFROM_CCASFLAGS \
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
	-Wl,-T,$(top_srcdir)/boot/bootiso.ld \
	$(LINKER_SEL) -static \
	$(NOSTDLIB_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS)

EXTRA_bootiso_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/bootiso.ld

bootiso_elf_LDADD = \
	libbootsect.a \
	$(LIBGCC_ELF32)

# bootiso-bin

$(top_builddir)/bootiso-bin: $(top_builddir)/bootiso-elf
	$(OBJCOPY) -O binary --only-section=.text "$<" "$@"

# bootpxe-bios

bin_PROGRAMS += bootpxe-bios-elf
generate_symbols_list += bootpxe-bios-elf

bootpxe_bios_elf_SOURCES = \
	boot/bootpxe-bios.ld \
	boot/mpentry_bios.S \
	boot/pxe-bios.S \
	boot/pxe.h \
	boot/pxemain.cc \
	boot/pxemain.h \
	boot/pxemain_bios.cc \
	boot/pxemain_bios.h \
	boot/malloc_bios.cc

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
	-Wl,-T,$(top_srcdir)/boot/bootpxe-bios.ld \
	-Wl,-Map,$(top_builddir)/bootpxe-bios.map
	$(LINKER_SEL) -static -Bsymbolic \
	$(NOSTDLIB_FLAGS) \
	$(COMPILER_FLAGS)

bootpxe_bios_elf_LDADD = \
	libbootsect.a \
	$(LIBGCC_ELF32)

EXTRA_bootpxe_bios_elf_DEPENDENCIES = \
	boot/bootpxe-bios.ld

$(top_builddir)/bootpxe-bios.map: bootpxe-bios-elf

# bootpxe-bin

$(top_builddir)/bootpxe-bios-bin: $(top_builddir)/bootpxe-bios-elf
	$(OBJCOPY) -O binary --only-section=.text "$<" "$@"

# efi

# Workaround ld bugs that screw up position independent instruction relax
# Avoid PIC and use large model
BOOTLOADER_EFI_FLAGS = \
	-D__efi \
	-fshort-wchar \
	-I$(top_srcdir)/boot \
	-I$(top_srcdir)/boot/include \
	-Wl,--no-dynamic-linker \
	-mcmodel=large \
	$(INVISIBILITY_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(NO_REDZONE_FLAGS) \
	$(STACK_CHECK_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(NO_COMMON_FLAGS) \
	$(FREESTANDING_FLAGS)

#$(RETPOLINE_FLAGS)

bin_PROGRAMS += bootefi-amd64
generate_symbols_list += bootefi-amd64

bootefi_amd64_SOURCES = \
	boot/bootentry_efi.S \
	boot/bootefi.cc \
	boot/ctors.cc \
	boot/fs.cc \
	boot/cpu_efi.cc \
	boot/retpoline.S \
	boot/physmap.cc \
	boot/physmem_efi.cc \
	boot/cpu.cc \
	boot/cpuid.cc \
	boot/cpu64_efi.S \
	boot/debug.cc \
	boot/assert.cc \
	boot/string.cc \
	boot/string_char16.cc \
	boot/halt_efi.cc \
	boot/screen.cc \
	boot/screen_efi.cc \
	boot/malloc.cc \
	boot/malloc_efi.cc \
	boot/rand.cc \
	boot/paging.cc \
	boot/elf64.cc \
	boot/mpentry_efi.S \
	boot/progressbar.cc \
	boot/bootmenu.cc \
	boot/tui.cc \
	boot/tui_efi.cc \
	boot/include/boottable.h \
	boot/include/boottable_decl.h \
	boot/vesa.cc \
	boot/vesa_efi.cc \
	boot/boottable.cc \
	boot/boottable_efi.cc \
	boot/qemu.cc \
	boot/qemu.h \
	boot/utf.cc \
	boot/utf.h

#bootefi_i386_SOURCES = \
	$(bootefi_amd64_SOURCES)

bootefi_amd64_CFLAGS = \
	-DFROM_CFLAGS \
	-nostartfiles \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(PE64_FLAGS) \
	$(BOOTLOADER_EFI_FLAGS)

#bootefi_i386_CFLAGS = \
	$(BOOTLOADER_COMMON_FLAGS) \
	-DFROM_CFLAGS \
	$(PE32_FLAGS) \
	$(BOOTLOADER_EFI_FLAGS)

bootefi_amd64_CXXFLAGS = \
	-DFROM_CXXFLAGS \
	-nostartfiles \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(PE64_FLAGS) \
	$(BOOTLOADER_EFI_FLAGS)

#bootefi_i386_CXXFLAGS = \
	-DFROM_CXXFLAGS \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(PE32_FLAGS) \
	$(BOOTLOADER_EFI_FLAGS)

bootefi_amd64_CCASFLAGS = \
	-DFROM_CCASFLAGS \
	$(BOOTLOADER_COMMON_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(bootefi_amd64_CXXFLAGS)

#bootefi_i386_CCASFLAGS = \
	$(bootefi_amd64_CCASFLAGS)

bootefi_amd64_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(PE64_FLAGS) \
	$(LIBGCC_PE64) \
	-g \
	-Wl,-T,$(top_srcdir)/boot/bootefi.ld \
	-Wl,-Map,$(top_builddir)/bootefi-amd64.map \
	$(NOSTDLIB_FLAGS)

#-Wl,--subsystem,10 \
#-Wl,-pie \
#-Wl,-N \
#-Wl,-m,i386pep

#bootefi_i386_LDFLAGS = \
	-DFROM_LDFLAGS \
	$(PE32_FLAGS) \
	$(LIBGCC_PE32) \
	-g \
	-Wl,-N \
	-Wl,-m,i386pe \
	-Wl,-T,$(top_srcdir)/boot/bootefi.ld \
	-Wl,--subsystem,10 \
	-Wl,-Map,$(top_builddir)/bootefi-i386.map

bootefi_amd64_LDADD = \
   $(LIBGCC_PE64)

#bootefi_i386_LDADD = \
   $(LIBGCC_PE32)

EXTRA_bootefi_amd64_DEPENDENCIES = \
    $(top_srcdir)/boot/bootefi.ld

bootx64.efi: bootefi-amd64
	$(OBJCOPY) \
		--remove-section=.eh_frame \
		--remove-section=.debug_info \
		--remove-section=.debug_abbrev \
		--remove-section=.debug_ranges \
		--remove-section=.debug_aranges \
		--remove-section=.debug_line \
		--remove-section=.debug_str \
		--remove-section=.debug_macro \
		--remove-section=.debug_loc \
		$< $@

#bootia32.efi: bootefi-i386
#	$(OBJCOPY) \
#		--remove-section=.eh_frame \
#		--remove-section=.debug_info \
#		--remove-section=.debug_abbrev \
#		--remove-section=.debug_ranges \
#		--remove-section=.debug_aranges \
#		--remove-section=.debug_line \
#		--remove-section=.debug_str \
#		--remove-section=.debug_macro \
#		$< $@

# kernel

KERNEL_EXPLICIT_INSTANTIATIONS = \
	kernel/device/iocp.cc \
	kernel/device/iocp.h \
	kernel/lib/cc/vector.h \
	kernel/lib/cc/vector.cc \
	kernel/lib/cc/cxxstring.h \
	kernel/lib/cc/cxxstring.cc \
	kernel/lib/cc/chrono.h \
	kernel/lib/cc/chrono.cc \
	kernel/lib/cc/numeric.h \
	kernel/lib/cc/numeric.cc \
	kernel/lib/cc/functional.h \
	kernel/lib/cc/functional.cc \
	kernel/lib/cc/initializer_list.h \
	kernel/lib/cc/type_traits.h \
	kernel/lib/cc/type_traits.cc \
	kernel/lib/cc/cxxexception.h \
	kernel/lib/cc/cxxexception.cc \
	kernel/lib/cxxexcept.h \
	kernel/lib/cxxexcept.cc \
	kernel/lib/mutex.cc \
	kernel/lib/dev_storage.cc \
	kernel/lib/dev_storage.h

#$(KERNEL_EXPLICIT_INSTANTIATIONS)

KERNEL_SOURCES_SHARED = \
	kernel/arch/x86_64/entry.S \
	kernel/arch/x86_64/cpu/isr.S \
	kernel/arch/x86_64/cpu/except_asm.S \
	kernel/arch/x86_64/cpu/syscall.S \
	kernel/arch/x86_64/cpu/nofault.cc \
	kernel/arch/x86_64/cpu/nofault.h \
	kernel/arch/x86_64/cpu/phys_alloc.cc \
	kernel/arch/x86_64/cpu/phys_alloc.h \
	kernel/arch/x86_64/user_mem.cc \
	kernel/main.cc \
	kernel/lib/assert.cc \
	kernel/lib/assert.h \
	kernel/lib/bitop.h \
	kernel/lib/bitsearch.cc \
	kernel/lib/bitsearch.h \
	kernel/lib/bootinfo.cc \
	kernel/lib/bootinfo.h \
	kernel/lib/bsearch.cc \
	kernel/lib/bsearch.h \
	kernel/lib/bswap.cc \
	kernel/lib/bswap.h \
	kernel/lib/callout.cc \
	kernel/lib/callout.h \
	kernel/lib/radix_tree.h \
	kernel/lib/radix_tree.cc \
	kernel/lib/cc/algorithm.h \
	kernel/lib/cc/algorithm.cc \
	kernel/lib/cc/exception.h \
	kernel/lib/cc/exception.cc \
	kernel/lib/cc/memory.h \
	kernel/lib/cc/memory.cc \
	kernel/lib/cc/utility.h \
	kernel/lib/cc/utility.cc \
	kernel/lib/cc/permute.h \
	kernel/lib/cc/permute.cc \
	kernel/lib/conio.cc \
	kernel/lib/conio.h \
	kernel/lib/debug.cc \
	kernel/lib/debug.h \
	kernel/lib/desc_alloc.cc \
	kernel/lib/desc_alloc.h \
	kernel/lib/dev_eth.cc \
	kernel/lib/dev_eth.h \
	kernel/lib/dev_registration.h \
	kernel/lib/dev_char.cc \
	kernel/lib/dev_char.h \
	kernel/lib/dev_text.cc \
	kernel/lib/dev_text.h \
	kernel/lib/dev_graphics.cc \
	kernel/lib/dev_graphics.h \
	kernel/lib/dirent.h \
	kernel/lib/errno.h \
	kernel/lib/errno.cc \
	kernel/lib/export.h \
	kernel/lib/fileio.cc \
	kernel/lib/fileio.h \
	kernel/lib/framebuffer.cc \
	kernel/lib/framebuffer.h \
	kernel/lib/hash.cc \
	kernel/lib/hash.h \
	kernel/lib/hash_table.cc \
	kernel/lib/hash_table.h \
	kernel/lib/heap.cc \
	kernel/lib/heap.h \
	kernel/lib/intrin_compat.h \
	kernel/lib/irq.cc \
	kernel/lib/irq.h \
	kernel/lib/keyboard.cc \
	kernel/lib/keyboard.h \
	kernel/lib/likely.h \
	kernel/lib/main.h \
	kernel/lib/math.cc \
	kernel/lib/math.h \
	kernel/lib/mm.cc \
	kernel/lib/mm.h \
	kernel/lib/mouse.cc \
	kernel/lib/mouse.h \
	kernel/lib/png.cc \
	kernel/lib/png.h \
	kernel/lib/pool.cc \
	kernel/lib/pool.h \
	kernel/lib/printk.cc \
	kernel/lib/printk.h \
	kernel/lib/priorityqueue.cc \
	kernel/lib/priorityqueue.h \
	kernel/lib/process.cc \
	kernel/lib/process.h \
	kernel/lib/rand.cc \
	kernel/lib/rand.h \
	kernel/lib/rbtree.h \
	kernel/lib/rbtree.cc \
	kernel/lib/basic_set.h \
	kernel/lib/basic_set.cc \
	kernel/lib/refcount.h \
	kernel/lib/refcount.cc \
	kernel/lib/stddef.h \
	kernel/lib/stdlib.cc \
	kernel/lib/stdlib.h \
	kernel/lib/string.cc \
	kernel/lib/string.h \
	kernel/lib/sys/types.h \
	kernel/lib/thread.cc \
	kernel/lib/thread.h \
	kernel/lib/threadsync.cc \
	kernel/lib/threadsync.h \
	kernel/lib/time.cc \
	kernel/lib/time.h \
	kernel/lib/unique_ptr.cc \
	kernel/lib/unique_ptr.h \
	kernel/lib/unistd.h \
	kernel/lib/work_queue.cc \
	kernel/lib/zlib_helper.cc \
	kernel/lib/zlib_helper.h \
	kernel/lib/asan.cc \
	kernel/lib/ubsan.cc \
	kernel/lib/contig_alloc.cc \
	kernel/lib/contig_alloc.h \
	kernel/lib/user_mem.h \
	kernel/lib/uleb.cc \
	kernel/lib/uleb.h \
	kernel/lib/engunit.cc \
	kernel/lib/engunit.h \
	kernel/fs/ext4.cc \
	kernel/fs/devfs.cc \
	kernel/fs/tmpfs.cc \
	kernel/net/arp_frame.cc \
	kernel/net/arp_frame.h \
	kernel/net/arp.h \
	kernel/net/dhcp.cc \
	kernel/net/dhcp.h \
	kernel/net/ethernet.h \
	kernel/net/eth_frame.cc \
	kernel/net/eth_frame.h \
	kernel/net/eth_q.cc \
	kernel/net/eth_q.h \
	kernel/net/icmp.cc \
	kernel/net/icmp_frame.cc \
	kernel/net/icmp_frame.h \
	kernel/net/icmp.h \
	kernel/net/ipv4.cc \
	kernel/net/ipv4_frame.cc \
	kernel/net/ipv4_frame.h \
	kernel/net/ipv4.h \
	kernel/net/tcp.cc \
	kernel/net/tcp_frame.cc \
	kernel/net/tcp_frame.h \
	kernel/net/tcp.h \
	kernel/net/udp.cc \
	kernel/net/udp_frame.cc \
	kernel/net/udp_frame.h \
	kernel/net/udp.h \
	kernel/arch/cpu.h \
	kernel/arch/mmu.h \
	kernel/arch/x86_64/bios_data.cc \
	kernel/arch/x86_64/bios_data.h \
	kernel/arch/x86_64/cpu/apic.cc \
	kernel/arch/x86_64/cpu/apic.h \
	kernel/arch/x86_64/cpu/ioapic.cc \
	kernel/arch/x86_64/cpu/ioapic.h \
	kernel/arch/x86_64/cpu/mptables.h \
	kernel/arch/x86_64/cpu/asm_constants.h \
	kernel/arch/x86_64/cpu/atomic.cc \
	kernel/arch/x86_64/cpu/atomic.h \
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
	kernel/arch/x86_64/cpu/halt.cc \
	kernel/arch/x86_64/cpu/halt.h \
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
	kernel/arch/x86_64/cpu/spinlock.cc \
	kernel/arch/x86_64/cpu/spinlock.h \
	kernel/arch/x86_64/cpu/spinlock_arch.h \
	kernel/arch/x86_64/cpu/syscall_dispatch.cc \
	kernel/arch/x86_64/cpu/syscall_dispatch.h \
	kernel/arch/x86_64/cpu/thread_impl.cc \
	kernel/arch/x86_64/cpu/thread_impl.h \
	kernel/device/acpigas.cc \
	kernel/device/acpigas.h \
	kernel/device/acpihw.cc \
	kernel/device/acpihw.h \
	kernel/device/ata.cc \
	kernel/device/ata.h \
	kernel/device/ata.h \
	kernel/device/e9debug.cc \
	kernel/device/e9debug.h \
	kernel/device/eainstrument.cc \
	kernel/device/eainstrument.h \
	kernel/device/e9debug.h \
	kernel/device/i350.cc \
	kernel/device/keyb8042_layout/keyb8042_layout_us.cc \
	kernel/device/pci.cc \
	kernel/device/pci.h \
	kernel/device/pci.h \
	kernel/device/serial-uart.cc \
	kernel/device/serial-uart.h \
	kernel/device/serial-uart.h \
	kernel/device/usb.h	\
	kernel/device/usb.cc \
	kernel/device/dev_usb_ctl.cc \
	kernel/device/usb_hid.h \
	kernel/device/usb_hid.cc \
	kernel/device/usb_audio.h \
	kernel/device/usb_audio.cc \
	kernel/device/usb_hub.h \
	kernel/device/usb_hub.cc \
	kernel/device/vga.cc \
	kernel/device/vt100.cc \
	kernel/device/vt100.h \
	kernel/device/vt100_out.cc \
	kernel/device/vt100_out.h \
	kernel/arch/x86_64/elf64.cc \
	kernel/arch/x86_64/elf64_decl.h \
	kernel/arch/x86_64/elf64.h \
	kernel/arch/x86_64/gdbstub.cc \
	kernel/arch/x86_64/gdbstub.h \
	kernel/arch/x86_64/nano_time.cc \
	kernel/arch/x86_64/stacktrace.cc \
	kernel/arch/x86_64/stacktrace.h \
	kernel/arch/x86_64/types.h \
	kernel/syscall/syscall_helper.h \
	kernel/syscall/syscall_helper.cc \
	kernel/syscall/sys_fd.cc \
	kernel/syscall/sys_mem.cc \
	kernel/syscall/sys_time.cc \
	kernel/syscall/sys_process.cc \
	kernel/syscall/sys_module.cc \
	kernel/syscall/sys_sys.cc \
	kernel/syscall/sys_render.cc

KERNEL_INCLUDES_SHARED = \
	-I$(top_srcdir)/boot/include \
	-I$(top_srcdir)/kernel \
	-I$(top_srcdir)/kernel/lib \
	-I$(top_srcdir)/kernel/net \
	-I$(top_srcdir)/kernel/lib/cc \
	-I$(top_srcdir)/kernel/arch \
	-I$(top_srcdir)/kernel/arch/x86_64

KERNEL_CXXFLAGS_SHARED = \
	-DFROM_CXXFLAGS \
	-D__DGOS__ \
	-D__DGOS_KERNEL__=0x00000001 \
	-fPIE \
	$(FREESTANDING_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(NO_REDZONE_FLAGS) \
	$(NO_STACK_CHECK_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(YES_EXCEPTIONS) \
	$(NO_COMMON_FLAGS) \
	$(INVISIBILITY_FLAGS) \
	-export-dynamic \
	$(NO_FLOAT_FLAGS) \
	$(SANITIZE_UNDEFINED_FLAGS) \
	$(WARN_STACK_USAGE_FLAGS)

#-fsanitize-undefined-trap-on-error
#$(NO_EXCEPTIONS_FLAGS)
#$(NO_RTTI_FLAGS)

#$(RETPOLINE_FLAGS)

# Buggy
#$(STACK_CHECK_FLAGS)

KERNEL_CCASFLAGS_SHARED = \
	-DFROM_CCASFLAGS \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS)

KERNEL_LDFLAGS_SHARED = \
	-DFROM_LDFLAGS \
	$(ELF64_FLAGS) \
	-Wl,-T,$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	$(LINKER_DEBUG) \
	-Wl,-g \
	-export-dynamic \
	-Wl,--gc-keep-exported \
	-Wl,-pie \
	-Wl,--whole-archive -Wl,libkei.a -Wl,--no-whole-archive \
	-Wl,--no-strip-discarded \
	$(NOSTDLIB_FLAGS)

#-Wl,-m,elf_x86_64
#-Wl,--no-dynamic-linker

#-Wl,-export-dynamic

EXTRA_KERNEL_DEPENDENCIES_SHARED = \
	$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	$(top_builddir)/u_vga16.o \
	$(top_builddir)/libkei.a \
	$(DUMPDIR)


KERNEL_LDADD_SHARED = -lgcc $(top_builddir)/u_vga16.o

# ----------------------------------------------------------------------------
# Explicit instantiations because __attribute__((__visibility__(...))) is broken

noinst_LIBRARIES += libkei.a

libkei_a_SOURCES = \
	$(KERNEL_EXPLICIT_INSTANTIATIONS)

libkei_a_CXXFLAGS = \
	$(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(VISIBILITY_FLAGS) \
	-DLIBKEI

# ----------------------------------------------------------------------------
# kernel variations

KERNEL_TRACING_FLAGS = \
	$(INSTRUMENT_FUNCTIONS_FLAGS) \
	-Wl,-Map,$(top_builddir)/kernel-tracing.map

KERNEL_ASAN_FLAGS = \
	$(ASAN_FLAGS) \
	-Wl,-Map,$(top_builddir)/kernel-asan.map

KERNEL_GENERIC_FLAGS = \
	-Wl,-Map,$(top_builddir)/kernel-generic.map

bin_PROGRAMS += kernel-generic
generate_symbols_list += kernel-generic
generate_kallsym_list += kernel-generic

kernel_generic_SOURCES = $(KERNEL_SOURCES_SHARED)

kernel_generic_CXXFLAGS = \
	$(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_GENERIC_FLAGS)

kernel_generic_CCASFLAGS = \
	$(KERNEL_CCASFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_GENERIC_FLAGS)

kernel_generic_LDFLAGS = \
	$(KERNEL_LDFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_GENERIC_FLAGS)

kernel_generic_LDADD = \
	$(KERNEL_LDADD_SHARED)

EXTRA_kernel_generic_DEPENDENCIES = \
	$(EXTRA_KERNEL_DEPENDENCIES_SHARED)

bin_PROGRAMS += kernel-tracing
generate_symbols_list += kernel-tracing
generate_kallsym_list += kernel-tracing

kernel_tracing_SOURCES = \
	$(KERNEL_SOURCES_SHARED)

kernel_tracing_CXXFLAGS = \
	$(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_TRACING_FLAGS)

kernel_tracing_CFLAGS = \
	$(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_TRACING_FLAGS)

kernel_tracing_CCASFLAGS = \
	$(KERNEL_CCASFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_TRACING_FLAGS)

kernel_tracing_LDFLAGS = \
	$(KERNEL_LDFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(KERNEL_TRACING_FLAGS)

kernel_tracing_LDADD = $(KERNEL_LDADD_SHARED)

EXTRA_kernel_tracing_DEPENDENCIES = \
	$(EXTRA_KERNEL_DEPENDENCIES_SHARED)

bin_PROGRAMS += kernel-asan
generate_symbols_list += kernel-asan
generate_kallsym_list += kernel-asan

kernel_asan_SOURCES = \
	$(KERNEL_SOURCES_SHARED)

kernel_asan_CXXFLAGS = \
	$(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(ASAN_FLAGS)

kernel_asan_CFLAGS = \
	$(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(ASAN_FLAGS)

kernel_asan_CCASFLAGS = \
	$(KERNEL_CCASFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(ASAN_FLAGS)

kernel_asan_LDFLAGS = \
	$(KERNEL_LDFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) \
	$(ASAN_FLAGS)

kernel_asan_LDADD = $(KERNEL_LDADD_SHARED)

EXTRA_kernel_asan_DEPENDENCIES = \
	$(EXTRA_KERNEL_DEPENDENCIES_SHARED)

$(top_builddir)/dgos-kernel-generic: $(top_builddir)/kernel-generic
	$(LN) -f "$<" "$@"

$(top_builddir)/dgos-kernel-tracing: $(top_builddir)/kernel-tracing
	$(LN) -f "$<" "$@"

$(top_builddir)/dgos-kernel-asan: $(top_builddir)/kernel-asan
	$(LN) -f "$<" "$@"

# ----------------------------------------------------------------------------
# parse-trace

trace-tool$(EXEEXT): $(top_srcdir)/pif-trace/pif-trace.cc
	$(CXX_FOR_BUILD) -ggdb3 -o "$@" \
		-D_GNU_SOURCE -D_DEFAULT_SOURCE \
		-std=c++17 \
		-fwhole-program -pthread \
		-O3 -march=native \
		-Wall -Wextra -Wpedantic \
		$^ \
		-Wl,--start-group \
		-lncurses -ltinfo -lgpm -ldl -lz \
		-Wl,--end-group

#-O3 -march=native

# ----------------------------------------------------------------------------
#


# ----------------------------------------------------------------------------
# crt0

INCLUDES_NAMES = $(shell cd $(top_srcdir)/libc/include && $(FIND) -type f)
INCLUDES_DEPLOYED = \
	$(patsubst %,$(top_builddir)/sysroot/include/%,$(INCLUDES_NAMES))

sysroot/lib/:
	$(MKDIR) -p $@

define compilestartup =
STARTUPFILES += $(top_builddir)/sysroot/lib/$(1)

$(top_builddir)/sysroot/lib/$(1): \
		$(2) \
		$(top_srcdir)/libc/include/sys/cpu_bug.h \
		$(INCLUDES_DEPLOYED)
	$$(MKDIR) -p "$$(@D)" && \
		$$(CXX) \
			-g \
			$$(ASM_DEBUG_INFO_FLAGS) \
			--sysroot $(top_builddir)/sysroot \
			-isystem $(top_builddir)/sysroot/include \
			-o $$@ \
			-c $$<

endef

$(eval $(call \
	compilestartup,crt0.o,$(top_srcdir)/user/crt0/crt0.S \
))
$(eval $(call \
	compilestartup,crt0pc80.o,$(top_srcdir)/user/crt0/crt0pc80.S \
))
$(eval $(call \
	compilestartup,crt0pc64.o,$(top_srcdir)/user/crt0/crt0pc64.S \
))
$(eval $(call \
	compilestartup,crt0pc32.o,$(top_srcdir)/user/crt0/crt0pc32.S \
))
$(eval $(call \
	compilestartup,x32-crt0pc80.o,$(top_srcdir)/user/crt0/x32-crt0pc80.S \
))
$(eval $(call \
	compilestartup,x32-crt0pc64.o,$(top_srcdir)/user/crt0/x32-crt0pc64.S \
))
$(eval $(call \
	compilestartup,x32-crt0pc32.o,$(top_srcdir)/user/crt0/x32-crt0pc32.S \
))

#

define deploy_stdlib=

$(top_builddir)/sysroot/lib/lib$(1).a: $(top_builddir)/lib$(1).a
	$$(MKDIR) -p $$(@D) && $$(CP) "$$<" "$$@"

endef

$(foreach name,$(STDLIB_FILE_NAMES),$(eval $(call deploy_stdlib,$(name))))

define deploy_sysroot_header =
$$(top_builddir)/sysroot/include/$(1): $$(top_srcdir)/libc/include/$(1)
	$(MKDIR) -p $$(@D)
	$(CP) -f "$$<" "$$@"

endef

$(foreach header,$(INCLUDES_NAMES),$(eval \
	$(call deploy_sysroot_header,$(header)) \
))

startupcode: $(STARTUPFILES)

.PHONY: startupcode

# ----------------------------------------------------------------------------
# Shell

bin_PROGRAMS += shell
generate_symbols_list += shell

shell_SOURCES = \
	user/shell/grammar.c \
	user/shell/parser.h \
	user/shell/parser.cc

shell_CXXFLAGS = \
	-DFROMCXXFLAGS  \
	-isystem $(top_builddir)/sysroot/include \
	-g \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

#$(RETPOLINE_FLAGS)

shell_CFLAGS = \
	$(shell_CXXFLAGS)

shell_CCASFLAGS =
shell_LDFLAGS = \
	-Wl,-Map,$(top_builddir)/shell.map

#-Wl,--orphan-handling,warn

shell_LDADD =

#-v -Wl,-verbose

$(top_srcdir)/user/shell/grammar.c \
	$(top_srcdir)/user/shell/grammar.h \
	$(top_srcdir)/user/shell/grammar.out: \
	$(top_srcdir)/user/shell/grammar.y
	$(RM) -rf $@
	lemon $<

EXTRA_shell_DEPENDENCIES = \
	$(top_srcdir)/user/user64_phdrs.ld \
	$(top_srcdir)/user/shell/grammar.c \
	$(ALL_STDLIB_INSTALLED) \
	$(INCLUDES_DEPLOYED)

# ----------------------------------------------------------------------------
# libc

libc_adir = $(includedir)/libc

noinst_LIBRARIES += libc.a

libc_a_HEADERS = \
	libc/include/termios.h \
	libc/include/complex.h \
	libc/include/regex.h \
	libc/include/cpio.h \
	libc/include/pwd.h \
	libc/include/net/if.h \
	libc/include/wctype.h \
	libc/include/monetary.h \
	libc/include/nl_types.h \
	libc/include/wordexp.h \
	libc/include/mqueue.h \
	libc/include/assert.h \
	libc/include/syslog.h \
	libc/include/signal.h \
	libc/include/unistd.h \
	libc/include/stropts.h \
	libc/include/search.h \
	libc/include/limits.h \
	libc/include/libgen.h \
	libc/include/glob.h \
	libc/include/ulimit.h \
	libc/include/utime.h \
	libc/include/fnmatch.h \
	libc/include/spawn.h \
	libc/include/iconv.h \
	libc/include/fcntl.h \
	libc/include/stdio.h \
	libc/include/stdint.h \
	libc/include/setjmp.h \
	libc/include/sys/cpu_bug.h \
	libc/include/sys/syscall.h \
	libc/include/sys/resource.h \
	libc/include/sys/select.h \
	libc/include/sys/sem.h \
	libc/include/sys/socket.h \
	libc/include/sys/types.h \
	libc/include/sys/un.h \
	libc/include/sys/wait.h \
	libc/include/sys/utsname.h \
	libc/include/sys/stat.h \
	libc/include/sys/ipc.h \
	libc/include/sys/times.h \
	libc/include/sys/statvfs.h \
	libc/include/sys/msg.h \
	libc/include/sys/shm.h \
	libc/include/sys/time.h \
	libc/include/sys/cdefs.h \
	libc/include/sys/mman.h \
	libc/include/sys/uio.h \
	libc/include/semaphore.h \
	libc/include/arpa/inet.h \
	libc/include/fmtmsg.h \
	libc/include/pthread.h \
	libc/include/fenv.h \
	libc/include/aio.h \
	libc/include/dlfcn.h \
	libc/include/math.h \
	libc/include/poll.h \
	libc/include/wchar.h \
	libc/include/utmpx.h \
	libc/include/ctype.h \
	libc/include/sched.h \
	libc/include/tar.h \
	libc/include/ndbm.h \
	libc/include/time.h \
	libc/include/grp.h \
	libc/include/netdb.h \
	libc/include/string.h \
	libc/include/strings.h \
	libc/include/stdlib.h \
	libc/include/errno.h \
	libc/include/ftw.h \
	libc/include/inttypes.h \
	libc/include/dirent.h \
	libc/include/trace.h \
	libc/include/netinet/in.h \
	libc/include/netinet/tcp.h \
	libc/include/langinfo.h \
	libc/include/locale.h \
	libc/include/sys/socket.h \
	libc/include/sys/ioctl.h

libc_a_SOURCES = \
	libc/src/assert/assert.cc \
	libc/src/string/__bytebitmap.cc \
	libc/src/string/strsignal.cc \
	libc/src/string/strcoll_l.cc \
	libc/src/string/strerror.cc \
	libc/src/string/strlen.cc \
	libc/src/string/strdup.cc \
	libc/src/string/strtok.cc \
	libc/src/string/strnlen.cc \
	libc/src/string/strcpy.cc \
	libc/src/string/strcmp.cc \
	libc/src/string/memset.cc \
	libc/src/string/strndup.cc \
	libc/src/string/strcspn.cc \
	libc/src/string/memcmp.cc \
	libc/src/string/strrchr.cc \
	libc/src/string/strpbrk.cc \
	libc/src/string/memcpy.cc \
	libc/src/string/stpncpy.cc \
	libc/src/string/strtok_r.cc \
	libc/src/string/stpcpy.cc \
	libc/src/string/memccpy.cc \
	libc/src/string/strcat.cc \
	libc/src/string/strchr.cc \
	libc/src/string/strncpy.cc \
	libc/src/string/strncat.cc \
	libc/src/string/strstr.cc \
	libc/src/string/memmove.cc \
	libc/src/string/strspn.cc \
	libc/src/string/strcoll.cc \
	libc/src/string/strerror_r.cc \
	libc/src/string/strerror_l.cc \
	libc/src/string/strxfrm.cc \
	libc/src/string/memchr.cc \
	libc/src/string/strxfrm_l.cc \
	libc/src/string/strncmp.cc \
	libc/src/new/operator-new.cc \
	libc/src/dirent/opendir.cc \
	libc/src/dirent/closedir.cc \
	libc/src/dirent/fdopendir.cc \
	libc/src/dirent/readdir.cc \
	libc/src/dirent/rewinddir.cc \
	libc/src/dirent/telldir.cc \
	libc/src/dirent/dirfd.cc \
	libc/src/dirent/readdir_r.cc \
	libc/src/dirent/seekdir.cc \
	libc/src/stdlib/new.cc \
	libc/src/stdlib/_Exit.cc \
	libc/src/stdlib/abort.cc \
	libc/src/stdlib/abs.cc \
	libc/src/stdlib/malloc.cc \
	libc/src/stdlib/calloc.cc \
	libc/src/stdlib/free.cc \
	libc/src/fcntl/creat.cc \
	libc/src/fcntl/fcntl.cc \
	libc/src/fcntl/open.cc \
	libc/src/fcntl/openat.cc \
	libc/src/fcntl/posix_fadvise.cc \
	libc/src/fcntl/posix_fallocate.cc \
	libc/src/stdio/bits/cfile.cc \
	libc/src/stdio/bits/cfile.h \
	libc/src/stdio/bits/formatter.cc \
	libc/src/stdio/bits/formatter.h \
	libc/src/stdio/stdin_out_err.cc \
	libc/src/stdio/fdopen.cc \
	libc/src/stdio/fprintf.cc \
	libc/src/stdio/fputc.cc \
	libc/src/stdio/fgets.cc \
	libc/src/stdio/printf.cc \
	libc/src/stdio/vfprintf.cc \
	libc/src/stdio/vprintf.cc \
	libc/src/stdio/fread.cc \
	libc/src/stdio/fwrite.cc \
	libc/src/stdio/puts.cc \
	libc/src/sys/indirect-thunk.S \
	libc/src/sys/mman/mmap.cc \
	libc/src/sys/mman/munmap.cc \
	libc/src/sys/mman/mremap.cc \
	libc/src/sys/mman/mprotect.cc \
	libc/src/sys/mman/msync.cc \
	libc/src/sys/mman/mlock.cc \
	libc/src/sys/mman/munlock.cc \
	libc/src/sys/mman/madvise.cc \
	libc/src/sys/ioctl/ioctl.cc \
	libc/src/sys/tls/tls_get_addr.h \
	libc/src/sys/tls/tls_get_addr.cc \
	libc/src/unistd/access.cc \
	libc/src/unistd/alarm.cc \
	libc/src/unistd/chdir.cc \
	libc/src/unistd/chown.cc \
	libc/src/unistd/close.cc \
	libc/src/unistd/confstr.cc \
	libc/src/unistd/crypt.cc \
	libc/src/unistd/dup2.cc \
	libc/src/unistd/dup.cc \
	libc/src/unistd/encrypt.cc \
	libc/src/unistd/execl.cc \
	libc/src/unistd/execle.cc \
	libc/src/unistd/execlp.cc \
	libc/src/unistd/execv.cc \
	libc/src/unistd/execve.cc \
	libc/src/unistd/execvp.cc \
	libc/src/unistd/_exit.cc \
	libc/src/unistd/faccessat.cc \
	libc/src/unistd/fchdir.cc \
	libc/src/unistd/fchownat.cc \
	libc/src/unistd/fchown.cc \
	libc/src/unistd/fdatasync.cc \
	libc/src/unistd/fexecve.cc \
	libc/src/unistd/fork.cc \
	libc/src/unistd/fpathconf.cc \
	libc/src/unistd/fsync.cc \
	libc/src/unistd/ftruncate.cc \
	libc/src/unistd/getcwd.cc \
	libc/src/unistd/getegid.cc \
	libc/src/unistd/geteuid.cc \
	libc/src/unistd/getgid.cc \
	libc/src/unistd/getgroups.cc \
	libc/src/unistd/gethostid.cc \
	libc/src/unistd/gethostname.cc \
	libc/src/unistd/getlogin.cc \
	libc/src/unistd/getlogin_r.cc \
	libc/src/unistd/getopt.cc \
	libc/src/unistd/getpgid.cc \
	libc/src/unistd/getpgrp.cc \
	libc/src/unistd/getpid.cc \
	libc/src/unistd/getppid.cc \
	libc/src/unistd/getsid.cc \
	libc/src/unistd/getuid.cc \
	libc/src/unistd/isatty.cc \
	libc/src/unistd/lchown.cc \
	libc/src/unistd/linkat.cc \
	libc/src/unistd/link.cc \
	libc/src/unistd/lockf.cc \
	libc/src/unistd/lseek.cc \
	libc/src/unistd/nice.cc \
	libc/src/unistd/pathconf.cc \
	libc/src/unistd/pause.cc \
	libc/src/unistd/pipe.cc \
	libc/src/unistd/pread.cc \
	libc/src/unistd/pwrite.cc \
	libc/src/unistd/read.cc \
	libc/src/unistd/readlinkat.cc \
	libc/src/unistd/readlink.cc \
	libc/src/unistd/rmdir.cc \
	libc/src/unistd/setegid.cc \
	libc/src/unistd/seteuid.cc \
	libc/src/unistd/setgid.cc \
	libc/src/unistd/setpgid.cc \
	libc/src/unistd/setpgrp.cc \
	libc/src/unistd/setregid.cc \
	libc/src/unistd/setreuid.cc \
	libc/src/unistd/setsid.cc \
	libc/src/unistd/setuid.cc \
	libc/src/unistd/sleep.cc \
	libc/src/unistd/swab.cc \
	libc/src/unistd/symlinkat.cc \
	libc/src/unistd/symlink.cc \
	libc/src/unistd/sync.cc \
	libc/src/unistd/sysconf.cc \
	libc/src/unistd/tcgetpgrp.cc \
	libc/src/unistd/tcsetpgrp.cc \
	libc/src/unistd/truncate.cc \
	libc/src/unistd/ttyname.cc \
	libc/src/unistd/ttyname_r.cc \
	libc/src/unistd/unlinkat.cc \
	libc/src/unistd/unlink.cc \
	libc/src/unistd/write.cc \
	libc/src/errno/errno.cc \
	libc/src/posix_spawn/posix_spawn.cc \
	libc/src/posix_spawn/posix_spawnp.cc \
	libc/src/posix_spawn/posix_spawn_file_actions_init.cc \
	libc/src/posix_spawn/posix_spawn_file_actions_destroy.cc \
	libc/src/posix_spawn/posix_spawn_file_actions_addopen.cc \
	libc/src/posix_spawn/posix_spawn_file_actions_adddup2.cc \
	libc/src/posix_spawn/posix_spawn_file_actions_addclose.cc \
	libc/src/posix_spawn/posix_spawnattr_init.cc \
	libc/src/posix_spawn/posix_spawnattr_destroy.cc \
	libc/src/posix_spawn/posix_spawnattr_setsigdefault.cc \
	libc/src/posix_spawn/posix_spawnattr_getsigdefault.cc \
	libc/src/posix_spawn/posix_spawnattr_setsigmask.cc \
	libc/src/posix_spawn/posix_spawnattr_getsigmask.cc \
	libc/src/posix_spawn/posix_spawnattr_setschedparam.cc \
	libc/src/posix_spawn/posix_spawnattr_getschedparam.cc \
	libc/src/posix_spawn/posix_spawnattr_getschedpolicy.cc \
	libc/src/posix_spawn/posix_spawnattr_setschedpolicy.cc \
	libc/src/posix_spawn/posix_spawnattr_setflags.cc \
	libc/src/posix_spawn/posix_spawnattr_getflags.cc \
	libc/src/posix_spawn/posix_spawnattr_setpgroup.cc \
	libc/src/posix_spawn/posix_spawnattr_getpgroup.cc \
	libc/src/sys/socket/socket.cc \
	libc/src/sys/module/init_module.cc \
	libc/src/sys/module/probe_pci_for.cc \
	libc/src/sys/syscall0.S \
	libc/src/sys/syscall1.S \
	libc/src/sys/syscall2.S \
	libc/src/sys/syscall3.S \
	libc/src/sys/syscall4.S \
	libc/src/sys/syscall5.S \
	libc/src/sys/syscall6.S \
	$(libc_a_HEADERS)

libc_a_CXXFLAGS = \
	-isystem $(top_srcdir)/libc/include \
	$(FREESTANDING_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS)

bin_PROGRAMS += libc.so
generate_symbols_list += libc.so

libc_so_SOURCES = $(libc_a_SOURCES)

libc_so_CXXFLAGS = $(libc_a_CXXFLAGS) \
	$(USERLIB64_FLAGS) \
	-fPIC -shared -nostdlib -ftls-model=global-dynamic

libc_so_CFLAGS = $(libc_a_CFLAGS) \
	-fPIC -shared -nostdlib

libc_so_CCASFLAGS = $(libc_a_CCASFLAGS) \
	-fPIC -shared -nostdlib

libc_so_LDFLAGS = $(libc_a_LDFLAGS) -fno-pie -fPIC -shared -nostdlib \
	-isystem $(top_srcdir)/libc/include \
	$(FREESTANDING_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS) \
	$(USERLIB64_LDFLAGS) -Wl,-shared

EXTRA_libc_so_DEPENDENCIES = $(EXTRA_libc_a_DEPENDENCIES)

noinst_LIBRARIES += libm.a

libm_a_SOURCES = \
	libc/math/cos.cc \
	libc/math/sin.cc \
	libc/math/log.cc

libm_a_CXXFLAGS = \
	-isystem $(top_srcdir)/libc/include \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS)

bin_PROGRAMS += libm.so

libm_so_SOURCES = $(libm_a_SOURCES) -fPIC -shared -nostdlib
libm_so_CXXFLAGS = $(libm_a_CXXFLAGS) -fPIC -shared -nostdlib
libm_so_CFLAGS = $(libm_a_CFLAGS) -fPIC -shared -nostdlib
libm_so_CCASFLAGS = $(libm_a_CCASFLAGS) -fPIC -shared -nostdlib
libm_so_LDFLAGS = $(libm_a_LDFLAGS) -fPIC -shared -nostdlib
EXTRA_libm_so_DEPENDENCIES = $(EXTRA_libm_a_DEPENDENCIES)
libm_so_LDADD = -lgcc

noinst_LIBRARIES += libstdc++.a

libstdc___a_SOURCES = \
	libc/cxx/terminate.cc \
	libc/cxx/abi/cxa_allocate_exception.cc

libstdc___a_CXXFLAGS = \
	-isystem $(top_srcdir)/libc/include \
	$(FREESTANDING_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS)

noinst_PROGRAMS += libstdc++.so

libstdc___so_SOURCES = \
	libc/cxx/terminate.cc \
	libc/cxx/abi/cxa_allocate_exception.cc

libstdc___so_CXXFLAGS = \
	-isystem $(top_srcdir)/libc/include \
	$(FREESTANDING_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS) \
	$(USERLIB64_FLAGS)

libstdc___so_LDFLAGS = \
	$(USERLIB64_LDFLAGS)

# ----------------------------------------------------------------------------
# libc unit tests

LIBC_TESTS = \
	$(top_srcdir)/libc/tests/testassert.cc \
	$(top_srcdir)/libc/tests/test_string.cc

LIBC_CXXFLAGS = \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(FREESTANDING_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	-isystem $(top_srcdir)/libc/include \
	-I$(top_builddir) \
	-L$(top_builddir)

#$(NO_EXCEPTIONS_FLAGS)
#$(NO_EXCEPTIONS_FLAGS)
#$(NO_EXCEPTIONS_FLAGS)

$(top_builddir)/libc_tests: $(LIBC_TESTS) $(top_builddir)/sysroot/lib/libc.a
	echo "#define TERMESC_RED" \
		"\"$$(tput setaf 1 | sed 's/\x1b/\\x1b/g')\"" \
		> $(top_builddir)/termesc.h
	echo "#define TERMESC_GREEN" \
		"\"$$(tput setaf 2 | sed 's/\x1b/\\x1b/g')\"" \
		>> $(top_builddir)/termesc.h
	echo "#define TERMESC_BLUE" \
		"\"$$(tput setaf 4 | sed 's/\x1b/\\x1b/g')\"" \
		>> $(top_builddir)/termesc.h
	echo "#define TERMESC_WHITE" \
		"\"$$(tput setaf 7 | sed 's/\x1b/\\x1b/g')\"" \
		>> $(top_builddir)/termesc.h

	$(CXX_FOR_BUILD) \
		$(LIBC_CXXFLAGS) \
		-o libc_tests \
		$(LIBC_TESTS) \
		-static -nostdlib -g -fno-builtin \
		$(ALL_STDLIB_INSTALLED) -lgcc

test_compile: $(top_builddir)/libc_tests

test: test_compile
	$(top_builddir)/libc_tests

.PHONY: test_compile test

# ----------------------------------------------------------------------------
# iibg

noinst_LIBRARIES += libg.a

libg_a_SOURCES = \
	libc/libg/debug.cc

libg_a_CFLAGS = $(LIBC_CXXFLAGS)
libg_a_CXXFLAGS = $(LIBC_CXXFLAGS)
libg_a_CCASFLAGS = $(LIBC_CXXFLAGS)

#bin_PROGRAMS += libg.so
#libg_so_SOURCES = $(libg_a_SOURCES) -fPIC -shared -nostdlib
#libg_so_CXXFLAGS = $(libg_a_CXXFLAGS) -fPIC -shared -nostdlib
#libg_so_CFLAGS = $(libg_a_CFLAGS) -fPIC -shared -nostdlib
#libg_so_CCASFLAGS = $(libg_a_CCASFLAGS) -fPIC -shared -nostdlib
#libg_so_LDFLAGS = $(libg_a_LDFLAGS) -fPIC -shared -nostdlib
#EXTRA_libg_so_DEPENDENCIES = $(EXTRA_libg_a_DEPENDENCIES)

# ----------------------------------------------------------------------------
# User programs

bin_PROGRAMS += dhcpclient
generate_symbols_list += dhcpclient

dhcpclient_SOURCES = \
	user/bin/dhcpclient/main.cc

dhcpclient_CXXFLAGS = -isystem sysroot/include

EXTRA_dhcpclient_DEPENDENCIES = \
	$(ALL_STDLIB_INSTALLED) \
	$(INCLUDES_DEPLOYED)

bin_PROGRAMS += ls
generate_symbols_list += ls

ls_SOURCES = \
	user/bin/ls/ls.cc

ls_CXXFLAGS = -isystem sysroot/include

EXTRA_ls_DEPENDENCIES = \
	$(ALL_STDLIB_INSTALLED) \
	$(INCLUDES_DEPLOYED)

#==========
# Common module declarations

KERNEL_MODULE_CXXFLAGS_SHARED = \
	$(FREESTANDING_FLAGS) \
	$(NO_REDZONE_FLAGS) \
	-D__DGOS_KERNEL__ \
	$(INVISIBILITY_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(YES_EXCEPTIONS) \
	-fPIC \
	-ggdb \
	-I$(top_srcdir)/boot/include \
	-I$(top_srcdir)/kernel \
	-I$(top_srcdir)/kernel/lib \
	-I$(top_srcdir)/kernel/net \
	-I$(top_srcdir)/kernel/lib/cc \
	-I$(top_srcdir)/kernel/arch \
	-I$(top_srcdir)/kernel/arch/x86_64 \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(NO_COMMON_FLAGS) \
	$(COMPILER_FLAGS) \
	$(NO_FLOAT_FLAGS) \
	$(SANITIZE_UNDEFINED_FLAGS) \
	$(WARN_STACK_USAGE_FLAGS)

#$(NO_EXCEPTIONS_FLAGS)
#$(NO_RTTI_FLAGS)
#$(RETPOLINE_FLAGS)

KERNEL_MODULE_LDFLAGS_SHARED = \
	-Wl,-shared \
	-Wl,-T$(top_srcdir)/kernel/arch/x86_64/module.ld \
	-Wl,-z,max-page-size=4096 \
	-Wl,--relax \
	-Wl,--eh-frame-hdr \
	$(LINKER_DEBUG) \
	$(NOSTDLIB_FLAGS)

KERNEL_MODULE_LDADD_SHARED = \
	$(top_builddir)/libkm.a

KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED = \
	$(top_srcdir)/kernel/arch/x86_64/module.ld \
	$(top_builddir)/libkm.a

KERNEL_MODULE_LDFLAGS_FN = \
	$(KERNEL_MODULE_LDFLAGS_SHARED) \
	-Wl,-Map,$(top_builddir)/$(1).km.map \
	-Wl,-z,now \
	$(top_builddir)/libkm.a

#==========

noinst_LIBRARIES += libkm.a

libkm_a_SOURCES = \
	modules/libkm/module_entry.cc \
	modules/libkm/module_main.cc \
	modules/libkm/dso_handle.S \
	modules/libkm/__cxa_pure_virtual.S \
	modules/libkm/unwind_resume.S

libkm_a_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED) -static

EXTRA_libkm_a_DEPENDENCIES =

#==========

bin_PROGRAMS += symsrv.km
generate_symbols_list += symsrv.km
generate_kallsym_list += symsrv.km

symsrv_km_SOURCES = \
	kernel/device/symbol_server.cc \
	kernel/device/symbol_server.h

symsrv_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

symsrv_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,symsrv)

symsrv_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_symsrv_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += usbxhci.km
generate_symbols_list += usbxhci.km
generate_kallsym_list += usbxhci.km

usbxhci_km_SOURCES = \
	kernel/device/usb_xhci.cc

usbxhci_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

usbxhci_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,usbxhci)

usbxhci_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_usbxhci_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += usbmsc.km
generate_symbols_list += usbmsc.km
generate_kallsym_list += usbmsc.km

usbmsc_km_SOURCES = \
	kernel/device/usb_storage.cc \
	kernel/device/usb_storage.h

usbmsc_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

usbmsc_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,usbmsc)

usbmsc_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_usbmsc_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += rtl8139.km
generate_symbols_list += rtl8139.km
generate_kallsym_list += rtl8139.km

rtl8139_km_SOURCES = \
	kernel/device/rtl8139.cc

rtl8139_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

rtl8139_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,rtl8193)

rtl8139_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_rtl8139_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += nvme.km
generate_symbols_list += nvme.km
generate_kallsym_list += nvme.km

nvme_km_SOURCES = \
	kernel/device/nvme.cc \
	kernel/device/nvme.h

nvme_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

nvme_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,nvme)

nvme_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_nvme_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += unittest.km
generate_symbols_list += unittest.km
generate_kallsym_list += unittest.km

unittest_km_SOURCES = \
	kernel/unittest/unittest.cc \
	kernel/unittest/unittest.h \
	kernel/unittest/test_threads.cc \
	kernel/unittest/test_allocator.cc \
	kernel/unittest/test_bit.cc \
	kernel/unittest/test_malloc.cc \
	kernel/unittest/test_sort.cc \
	kernel/unittest/test_vector.cc \
	kernel/unittest/test_set.cc \
	kernel/unittest/test_string.cc \
	kernel/unittest/test_printk.cc \
	kernel/unittest/test_chrono.cc \
	kernel/unittest/test_nofault.cc \
	kernel/unittest/test_block.cc \
	kernel/unittest/test_filesystem.cc

unittest_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

unittest_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,unittest)

unittest_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_unittest_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += keyb8042.km
generate_symbols_list += keyb8042.km
generate_kallsym_list += keyb8042.km

keyb8042_km_SOURCES = \
	kernel/device/keyb8042.cc

keyb8042_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

keyb8042_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,keyb8042)

keyb8042_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_keyb8042_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += ahci.km
generate_symbols_list += ahci.km
generate_kallsym_list += ahci.km

ahci_km_SOURCES = \
	kernel/device/ahci.cc

ahci_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

ahci_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,ahci)

ahci_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_ahci_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += ide.km
generate_symbols_list += ide.km
generate_kallsym_list += ide.km


ide_km_SOURCES = \
	kernel/device/ide.cc

ide_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

ide_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,ide)

ide_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_ide_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += virtio-base.km
generate_symbols_list += virtio-base.km
generate_kallsym_list += virtio-base.km

virtio_base_km_SOURCES = \
	kernel/device/virtio-base.cc \
	kernel/device/virtio-base.h

virtio_base_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

virtio_base_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,virtio-base)

virtio_base_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_virtio_base_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += virtio-blk.km
generate_symbols_list += virtio-blk.km
generate_kallsym_list += virtio-blk.km


virtio_blk_km_SOURCES = \
	kernel/device/virtio-blk.cc \
	kernel/device/virtio-blk.h

virtio_blk_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

virtio_blk_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,virtio-blk)

virtio_blk_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED) virtio-base.km

EXTRA_virtio_blk_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED) \
	$(top_builddir)/virtio-base.km

#==========

bin_PROGRAMS += virtio-gpu.km
generate_symbols_list += virtio-gpu.km
generate_kallsym_list += virtio-gpu.km

virtio_gpu_km_SOURCES = \
	kernel/device/virtio-gpu.cc \
	kernel/device/virtio-gpu.h

virtio_gpu_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

virtio_gpu_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,virtio-gpu)

virtio_gpu_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED) virtio-base.km

EXTRA_virtio_gpu_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED) \
	$(top_builddir)/virtio-base.km

#==========

bin_PROGRAMS += fat32.km
generate_symbols_list += fat32.km
generate_kallsym_list += fat32.km

fat32_km_SOURCES = \
	kernel/fs/fat32.cc \
	kernel/fs/fat32_decl.h

fat32_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

fat32_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,fat32)

fat32_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_fat32_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += iso9660.km
generate_symbols_list += iso9660.km
generate_kallsym_list += iso9660.km

iso9660_km_SOURCES = \
	kernel/fs/iso9660.cc \
	kernel/fs/iso9660_decl.h \
	kernel/fs/iso9660_part.cc

iso9660_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

iso9660_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,iso9660)

iso9660_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_iso9660_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += mbr.km
generate_symbols_list += mbr.km
generate_kallsym_list += mbr.km

mbr_km_SOURCES = \
	kernel/fs/mbr.cc

mbr_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

mbr_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,mbr)

mbr_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_mbr_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

bin_PROGRAMS += gpt.km
generate_symbols_list += gpt.km
generate_kallsym_list += gpt.km

gpt_km_SOURCES = \
	kernel/fs/gpt.cc

gpt_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

gpt_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,gpt)

gpt_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_gpt_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========

define strip_module =
modsym/$(1).km: $(1).km
	$(MKDIR) -p kmod
	mv -- "$<" "$@"
	$(STRIP) -g -o "$@" "$<"
endef

#==========

bin_PROGRAMS += user-shell
generate_symbols_list += user-shell

user_shell_CXXFLAGS = \
	-g \
	$(COMPILER_FLAGS)

user_shell_CFLAGS = \
	-g \
	$(COMPILER_FLAGS)

user_shell_SOURCES = \
	user/test/test.S

user_shell_LDFLAGS = \
	-Wl,-z,max-page-size=4096

user_shell_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS) \
	-isystem sysroot/include

EXTRA_user_shell_DEPENDENCIES = \
	$(ALL_STDLIB_INSTALLED)

#==========

bin_PROGRAMS += init
generate_symbols_list += init

init_CXXFLAGS = \
	-DFROMCXXFLAGS  \
	-ggdb \
	-isystem $(top_builddir)/sysroot/include \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

init_CFLAGS = \
	-DFROMCFLAGS  \
	-ggdb \
	-isystem $(top_builddir)/sysroot/include \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

init_SOURCES = \
	user/init/init.cc

init_LDFLAGS = -Wl,-Bdynamic
#-Wl,--no-eh-frame-hdr

init_LDADD =

init_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS) \
	-isystem $(top_builddir)/sysroot/include

EXTRA_init_DEPENDENCIES = \
	$(top_srcdir)/user/user64_phdrs.ld \
	$(ALL_STDLIB_INSTALLED) \
	$(INCLUDES_DEPLOYED)

#===========

bin_PROGRAMS += init-shared
generate_symbols_list += init-shared

init_shared_CXXFLAGS = \
	-DFROMCXXFLAGS  \
	-ggdb \
	-isystem $(top_builddir)/sysroot/include \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS) \
	-ftls-model=global-dynamic

init_shared_CFLAGS = \
	-DFROMCFLAGS  \
	-ggdb \
	-isystem $(top_builddir)/sysroot/include \
	$(COMPILER_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

init_shared_SOURCES = \
	user/init/init.cc

init_shared_LDFLAGS = -dynamic
#-Wl,--no-eh-frame-hdr

init_shared_LDADD =  libc.so

init_shared_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS) \
	-isystem $(top_builddir)/sysroot/include

EXTRA_init_shared_DEPENDENCIES = \
	$(top_srcdir)/user/user64_phdrs.ld \
	$(ALL_STDLIB_INSTALLED) \
	$(INCLUDES_DEPLOYED)


#===========

MODULE_FILENAMES = $(filter %.km,$(bin_PROGRAMS)) init
MODULE_LIST = $(subst %,$(top_builddir)/%,$(MODULE_FILENAMES))

SYMBOL_FILE = sym/$(1).sym

CPIO_LIST = $(MODULE_LIST) $(generate_symbols_outputs)

all-kernel: kernel-generic $(MODULE_LIST)

.PHONY: all-kernel

$(top_builddir)/sym/:
	$(MKDIR) -p "$@"

$(top_builddir)/initrd: $(CPIO_LIST)
	$(CPIO) -o --device-independent -H bin \
		< <($(patsubst %,echo "%";,$(CPIO_LIST))) > "$@"

# ----------------------------------------------------------------------------
# Misc make rules

$(DUMPDIR):
	$(MKDIR) -p $(top_builddir)/dump

#include zlib.mk
include $(top_srcdir)/symbols.mk
include $(top_srcdir)/diskfat.mk
include $(top_srcdir)/diskiso.mk

clean-local:
	rm -f $(top_builddir)/mbr-bin
	rm -f $(top_builddir)/bootfat-bin
	rm -f $(top_builddir)/bootiso-bin
	rm -f $(top_builddir)/bootpxe-bios-bin
	rm -f $(top_builddir)/trace-tool

	rm -f $(top_builddir)/bootefi-pe
	rm -f $(top_builddir)/boot*.efi

	rm -f $(top_builddir)/*.sym

	rm -f $(top_builddir)/*.map

	rm -f $(top_builddir)/*.dis.gz
	rm -f $(top_builddir)/bochs-debuglog
	rm -f $(top_builddir)/mbrdisk.img
	rm -f $(top_builddir)/gptdisk.img
	rm -f $(top_builddir)/fatpart.img
	rm -f $(top_builddir)/isodisk.img
	rm -f $(top_builddir)/initrd

	rm -f $(top_builddir)/boot1-bin
	rm -f $(top_builddir)/dgos-kernel-asan
	rm -f $(top_builddir)/dgos-kernel-generic
	rm -f $(top_builddir)/dgos-kernel-tracing
	rm -f $(top_builddir)/u_vga16.raw

distclean-local:
	rm -rf $(top_builddir)/extern

# ----------------------------------------------------------------------------
# Generate register definitions

BITFIELD_FILES = \
	$(top_srcdir)/kernel/device/nvme.bits \
	$(top_srcdir)/kernel/device/usb_hid.bits \
	$(top_srcdir)/kernel/device/usb_audio.bits \
	$(top_srcdir)/kernel/device/pci.bits \
	$(top_srcdir)/kernel/device/rtl8139.bits \
	$(top_srcdir)/kernel/device/i350.bits \
	$(top_srcdir)/kernel/device/usb_xhci.bits \
	$(top_srcdir)/kernel/arch/x86_64/cpu/apic.bits \
	$(top_srcdir)/kernel/arch/x86_64/cpu/pic.bits \
	$(top_srcdir)/kernel/arch/x86_64/cpu/perf_reg.bits \
	$(top_srcdir)/kernel/device/acpi.bits \
	$(top_srcdir)/kernel/device/ahci.bits \
	$(top_srcdir)/kernel/device/es173x.bits \
	$(top_srcdir)/kernel/net/ipv4.bits

# 1=path to bits file
define bitfield_gen =
$(1).h: $(1) $$(top_srcdir)/genregs.py
	rm -f "$$@"
	$$(top_srcdir)/genregs.py "$$<" > "$$@"
endef

$(foreach bitsfile,$(BITFIELD_FILES), \
	$(eval $(call bitfield_gen,$(bitsfile))) \
)

generated_headers = $(patsubst %,%.h,$(BITFIELD_FILES))

# ----------------------------------------------------------------------------
# Unicode VGA font

u_vga16.raw: u_vga16.bdf $(top_srcdir)/genfont.py
	$(top_srcdir)/genfont.py < $< > $@

u_vga16.o: u_vga16.raw
	$(LD) -r -b binary $< -o $@
	$(OBJCOPY) \
		--rename-section .data=.rodata \
		--set-section-flags .data=load,readonly,data \
		$@

# ----------------------------------------------------------------------------
# Randomize RAM content

#random-mem.img:
#	echo Building $(QEMU_RAM) random initial memory content image...
#	$(TRUNCATE) --size $(QEMU_RAM) "$@"
#	$(SHRED) --iterations=1 "$@"

# ----------------------------------------------------------------------------
# QEMU launcher

$(DUMPDIR)/qemu-debug-out:
	$(MKDIR) -p "$(@D)"
	mkfifo "$@"

$(top_builddir)/iso_stage:
	mkdir -p "$(top_builddir)/iso_stage"

$(DUMPDIR)/call-trace-out:
	$(MKDIR) -p "$(@D)"
	mkfifo "$@"

QEMU_EXTRA_DEPS = \
	$(DUMPDIR)/qemu-debug-out \
	$(DUMPDIR)/call-trace-out

#random-mem.img

QEMU_CPUS = $(shell $(NPROC))
QEMU_THREADS = 2
QEMU_CORES = $(shell echo $$(( $(QEMU_CPUS) / $(QEMU_THREADS) )))
QEMU_CPU = host,migratable=false,host-cache-info=true,fsgsbase

QEMU_RAM = 160M

QEMU_MEMFILL =
#QEMU_MEMFILL = \
#	-object memory-backend-file,id=ram-node0,mem-path="random-mem.img",size=$(QEMU_RAM),prealloc=no \
#	-numa node,nodeid=0,memdev=ram-node0

QEMU_USB_ROOT_PORTS ?= 16

# Puts keyboard, mouse, tablet on usb 2 root hub and audio on usb 3 root hub
QEMU_USB = \
	-device qemu-xhci,id=xhci,p2=$(QEMU_USB_ROOT_PORTS),p3=$(QEMU_USB_ROOT_PORTS),streams=on \
	-device usb-kbd,id=hidkbd,bus=xhci.0 \
	-device usb-tablet,id=hidtab,bus=xhci.0 \
	-device usb-mouse,id=hidmse,bus=xhci.0 \
	-device usb-audio,id=hidaud,bus=xhci.0 \
	$(QEMU_USB_EXTRA)

#-device usb-hub,id=hidhub,bus=xhci.0,ports=4,port=1
#-device usb-kbd,bus=hidhub.0 \
#-device usb-tablet,bus=hidhub.0 \
#-device usb-mouse,bus=hidhub.0

#-device usb-storage,bus=xhci.0,drive=stick
#-drive if=none,id=stick,file=usb_disk.img,format=raw,cache=none
#-device usb-audio,bus=xhci.0
#-device usb-net,bus=xhci.0
#-device usb-mtp,bus=xhci.0,x-root=$(builddir)
#-drive if=none,id=scsidrv,file=scsi_disk.img,format=raw,cache=none
#-device scsi-hd,drive=scsidrv
#-device usb-uas,bus=xhci.0,id=uas
#-device nec-usb-xhci,id=xhci,p2=16,p3=16,msix=on,streams=on
#-device usb-storage,bus=xhci.0,drive=stick

QEMU_NET = \
	-net nic,model=rtl8139,id=nic_rtl8139 \
	-net user \
	$(QEMU_BRIDGE)

#	-object filter-dump,file=dump/netdump_rtl8139.pcap,netdev=nic_rtl8139,id=pcap \
#	-nic model=ne2k_pci,id=nic_ne2k
#	-nic model=e1000,id=nic_e1000

QEMU_MONITOR = \
	-chardev socket,id=qemu-monitor,host=localhost,port=7777,server,nowait,telnet \
	-mon qemu-monitor,mode=readline

QEMU_DEBUGCON = \
	-chardev pipe,path=$(DUMPDIR)/qemu-debug-out,id=qemu-debug-out \
	-device isa-debugcon,chardev=qemu-debug-out \
	\
	-chardev pipe,path=$(DUMPDIR)/call-trace-out,id=call-trace-out \
	-device isa-debugcon,iobase=0xEA,chardev=call-trace-out \
	\
	-chardev stdio,id=seabios \
	-device isa-debugcon,iobase=0x402,chardev=seabios

#-chardev tty,id=qemu-serial-socket,path=/dev/ttyS0

QEMU_SERIAL = \
	-chardev socket,id=qemu-serial-socket,host=localhost,port=7788,server,nowait,nodelay \
	-device isa-serial,chardev=qemu-serial-socket,id=com1 \
	\
	-chardev socket,id=qemu-symsrv-socket,host=localhost,port=7789,server,nowait,nodelay \
	-device isa-serial,chardev=qemu-symsrv-socket,id=com2

QEMU_SOUND = -soundhw es1370

QEMU_COMMON = \
	$(QEMU_MONITOR) \
	$(QEMU_SERIAL) \
	$(QEMU_DEBUGCON) \
	$(QEMU_NET) \
	$(QEMU_SOUND) \
	-no-shutdown -no-reboot -d unimp,guest_errors \
	-m $(QEMU_RAM) \
	$(QEMU_FLAGS) \
	$(QEMU_USB) \
	$(QEMU_MEMFILL)

SECTOR_SZ = 512

QEMU_RT = $(QEMU_RT_$(1))
QEMU_RT_asan = -s -S -fw_cfg name=opt/com.doug16k.dgos.kernel_type,string=A
QEMU_RT_trace = -s -fw_cfg name=opt/com.doug16k.dgos.kernel_type,string=T
QEMU_RT_tracedbg = -S $(QEMU_RT_trace)
QEMU_RT_debug = -s -S
QEMU_RT_run = -s

QEMU_DRV_COMMON = \
	id=bootdrv,if=none,format=raw,werror=report,rerror=report,cache=none
QEMU_DRV = \
	-drive file=$(QEMU_IMAGE_$(1)),media=$(QEMU_MEDIA_$(1)),$(QEMU_DRV_COMMON)

#QEMU_MACHINE = $(QEMU_MACHINE_$(1))
QEMU_DISPLAY_SPICE = -spice ipv4,port=14446,disable-ticketing=on
QEMU_DISPLAY =
QEMU_MACHINE = -machine q35 $(QEMU_DISPLAY)

QEMU_BLKSIZE = logical_block_size=$(SECTOR_SZ),physical_block_size=$(SECTOR_SZ)

QEMU_DISK_TYPE_iso = ide-cd
QEMU_DISK_TYPE_mbr = ide-hd
QEMU_DISK_TYPE_gpt = ide-hd

QEMU_HDCTL_DEV_nvme = -device nvme,drive=bootdrv,serial=42,$(QEMU_BLKSIZE)
QEMU_HDCTL_DEV_virtio = \
	-device virtio-blk-pci,drive=bootdrv,$(QEMU_BLKSIZE),disable-legacy=on
#,num-queues=$(QEMU_CPUS)

#-device ich9-ahci,id=bootif

QEMU_HDCTL_DEV_ahci = \
	-device $(QEMU_DISK_TYPE_$(1)),$(QEMU_BLKSIZE),bus=ide.0,drive=bootdrv

QEMU_HDCTL_DEV_ide = -device piix4-ide,id=bootif \
	-device $(QEMU_DISK_TYPE_$(1)),bus=bootif.0,drive=bootdrv

QEMU_HDCTL_DEV_usb = \
	-device usb-storage,drive=bootdrv,bus=xhci.0,$(QEMU_BLKSIZE)

QEMU_HDCTL = $(call QEMU_MACHINE,$(1)) $(call QEMU_HDCTL_DEV_$(1),$(2))

QEMU_MEDIA = $(QEMU_MEDIA_$(1))
QEMU_MEDIA_gpt = disk
QEMU_MEDIA_mbr = disk
QEMU_MEDIA_iso = cdrom

QEMU_IMAGE = $(QEMU_IMAGE_$(1))
QEMU_IMAGE_gpt = gptdisk.img
QEMU_IMAGE_mbr = mbrdisk.img
QEMU_IMAGE_iso = isodisk.img

QEMU_NP = $(QEMU_NP_$(1))
QEMU_NP_smp = -smp cpus=$(QEMU_CPUS),cores=$(QEMU_CORES),threads=$(QEMU_THREADS)
QEMU_NP_up = -smp cpus=1,cores=1,threads=1
QEMU_NP_numa = \
    $(QEMU_NP_smp) \
    -numa node,cpus=0-$(shell echo $$(( $(QEMU_CPUS) / 2 - 1 ))) \
    -numa node,cpus=$(shell echo $$(( $(QEMU_CPUS) / 2 )))-$(shell echo $$(( $(QEMU_CPUS)-1 ))) \
    -numa dist,src=1,dst=0,val=25 \
    -numa dist,src=0,dst=1,val=15

QEMU_EMU = $(QEMU_EMU_$(1))
QEMU_EMU_kvm = -enable-kvm -cpu $(QEMU_CPU),invtsc,vmware-cpuid-freq,rdrand,fsgsbase
QEMU_EMU_tcg_BASE = -cpu max,migratable=false,fsgsbase,invtsc,vmware-cpuid-freq
QEMU_EMU_tcg = $(QEMU_EMU_tcg_BASE) --accel tcg,thread=single
QEMU_EMU_mttcg = $(QEMU_EMU_tcg_BASE) --accel tcg,thread=multi
#-cpu max,la57

QEMU_BOOT = $(QEMU_BOOT_$(1))
QEMU_BOOT_bios = -bios $(top_srcdir)/emu/seabios.rom
QEMU_BOOT_efi = -bios /usr/share/ovmf/OVMF.fd

QEMU_BOOTMEDIA = $(QEMU_BOOTMEDIA_$(1)_$(2))
QEMU_BOOTMEDIA_hd_bios = -boot c
QEMU_BOOTMEDIA_hd_efi = -boot c
QEMU_BOOTMEDIA_pxe_bios = -boot n \
	-net user,tftp=$(top_builddir),bootfile=bootpxe-bios-bin
QEMU_BOOTMEDIA_pxe_efi = \
	-boot n -net user,tftp=$(top_builddir),bootfile=bootx64.efi

QEMU_VIDEO = $(QEMU_VIDEO_$(1))
QEMU_VIDEO_vga = -vga std
QEMU_VIDEO_gl = \
	-global driver=virtio-vga,property=disable-legacy,value=on \
	-global driver=virtio-vga,property=disable-modern,value=off \
	-global driver=virtio-vga,property=x-disable-pcie,value=off \
	-global driver=virtio-vga,property=modern-pio-notify,value=on \
	-global driver=virtio-vga,property=max_outputs,value=16 \
	-global driver=virtio-vga,property=x-pcie-extcap-init,value=on \
	-global driver=virtio-vga,property=virgl,value=on \
	-vga virtio

# Connect to debug log
monitor-debug-output: $(DUMPDIR)/qemu-debug-out
	while true; do cat "$(DUMPDIR)/qemu-debug-out"; done

# Connect to console
monitor-connect:
	telnet localhost 7777

QL_RT = debug run trace asan tracedbg
QL_NP = up smp numa
QL_BT = bios efi
QL_BM = pxe hd
QL_FS = iso mbr gpt
QL_SC = ahci ide nvme usb virtio
QL_VO = vga gl
QL_EM = kvm tcg mttcg

# 1=rt, 2=np, 3=bt, 4=bm, 5=fs, 6=sc, 7=em
define qemu_launcher =
$(1)-$(2)-$(3)-$(4)-$(5)-$(6)-$(7)-$(8): \
		$$(call QEMU_IMAGE,$(5)) $$(QEMU_EXTRA_DEPS)
	echo -e -n '\ec'
	echo 'Run make monitor-debug-output in another terminal if progress stops'
	echo -e -n '\ec' >> dump/qemu-debug-out
	echo Running $$@
	$$(QEMU_SUDO) $$(QEMU) \
		$$(QEMU_EARLY) \
		$$(QEMU_COMMON) \
		$$(call QEMU_EMU,$(8)) \
		$$(call QEMU_DRV,$(5)) \
		$$(call QEMU_NP,$(2)) \
		$$(call QEMU_HDCTL,$(6),$(5)) \
		$$(call QEMU_BOOT,$(3)) \
		$$(call QEMU_RT,$(1)) \
		$$(call QEMU_BOOTMEDIA,$(4),$(3)) \
		$$(call QEMU_VIDEO,$(7)) \
		$$(QEMU_EXTRA)

.PHONY: $(1)-$(2)-$(3)-$(4)-$(5)-$(6)-$(7)-$(8)

endef

$(foreach rt,$(QL_RT), \
	$(foreach np,$(QL_NP), \
		$(foreach bt,$(QL_BT), \
			$(foreach bm,$(QL_BM), \
				$(foreach fs,$(QL_FS), \
					$(foreach sc,$(QL_SC), \
						$(foreach em,$(QL_EM), \
							$(foreach vo,$(QL_VO), \
$(eval $(call qemu_launcher,$(rt),$(np),$(bt),$(bm),$(fs),$(sc),$(vo),$(em))) \
							) \
						) \
					) \
				) \
			) \
		) \
	) \
)

debug-tcg: debug-smp-bios-hd-gpt-ahci-vga-tcg

.PHONY: debug-tcg

# ---
# Generate symbols

define genkallsym =
$$(top_builddir)/sym/$(1)-kallsyms: $(1) $$(top_srcdir)/Makefile.am
	$$(NM) --demangle --defined-only -n "$$<" > "$$@"

$$(top_builddir)/sym/$(1)-klinesyms: $(1) $$(top_srcdir)/Makefile.am
	$$(OBJDUMP) --dwarf=decodedline "$$<" | \
		$$(SED) -E 's/^(\S+)\s+([0-9]+)\s+(0x[0-9a-f]+)\s*x?/\1 \2 \3/g' | \
		grep -P '.*\s+\d+\s+0x[0-9a-f]+' > "$$@"

endef

$(foreach name,$(generate_kallsym_list),$(eval $(call genkallsym,$(name))))

define gensym =
$$(top_builddir)/sym/$(1).sym: $$(top_builddir)/$(1) \
		$$(top_srcdir)/symbols.mk $$(top_srcdir)/gensymtab.sh
	$$(MKDIR) -p $$(@D)
	OBJDUMP=$$(OBJDUMP) SORT=$(SORT) \
		$$(top_srcdir)/gensymtab.sh "$$(OBJDUMP)" e "$$@" "$$<"
endef

$(foreach name,$(generate_symbols_list),$(eval $(call gensym,$(name))))

generate_symbols_outputs = $(patsubst %,sym/%.sym,$(generate_symbols_list)) \
	$(patsubst %,sym/%-kallsyms,$(generate_kallsym_list)) \
	$(patsubst %,sym/%-klinesyms,$(generate_kallsym_list))

# ----------------------------------------------------------------------------
# Bochs launcher

BOCHS=bochs

# Generate the makefile targets for all the variations of bochs launch

BOCHS_MEDIA = gpt mbr iso
BOCHS_FW = bios efi
BOCHS_PART = kernel boot

# 1=media, 2=fw, 3=part
define bochs_launcher =
$(abs_top_builddir)/bochs-debugger-$(1)-$(2)-$(3)-commands: \
		$$(top_srcdir)/emu/bochs/bochs-debugger-$(1)-$(2)-$(3)-commands
	cp "$$<" "$$@"

debug-$(1)-$(2)-bochs-$(3): $(QEMU_IMAGE_$(1)) \
		$(SYMBOL_FILES_INSTALLED) \
		$$(top_srcdir)/emu/bochs/bochs-$(1)-$(2)-config.bxrc \
		$$(abs_top_builddir)/bochs-debugger-$(1)-$(2)-$(3)-commands
	$$(BOCHS) \
		-f $$(top_srcdir)/emu/bochs/bochs-$(1)-$(2)-config.bxrc \
		-rc $$(abs_top_builddir)/bochs-debugger-$(1)-$(2)-$(3)-commands \
		$$(BOCHS_FLAGS)
.PHONY: debug-$(1)-$(2)-bochs-$(3)
endef



$(foreach media,$(BOCHS_MEDIA), \
	$(foreach fw,$(BOCHS_FW), \
		$(foreach part,$(BOCHS_PART), \
			$(eval $(call bochs_launcher,$(media),$(fw),$(part))) \
		) \
	) \
)

debug: debug-up-bios-hd-gpt-ahci-vga-tcg

.PHONY: debug

debug-kvm: debug-up-bios-hd-gpt-ahci-vga-kvm

.PHONY: debug-kvm

run-kvm: run-up-bios-hd-gpt-ahci-vga-kvm

.PHONY: run-kvm

debug-bochs: debug-gpt-bios-bochs-kernel

.PHONY: debug-bochs

debug-bochs-boot: debug-gpt-bios-bochs-boot

.PHONY: debug-bochs-boot

run: run-up-bios-hd-gpt-ahci-vga-tcg

.PHONY: run
