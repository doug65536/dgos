## Process this file with automake to produce Makefile.in

BUILT_SOURCES = \
	$(ZLIBSTATICLIB) \
	$(ZLIBPUBLICINCLUDES) \
	qemu-emu.mk \
	$(abs_top_builddir)/dump \
	generated_headers

# Build libmbr
#  -> Link mbr-elf
#  -> Link mbr-bin
# Build libbootsect
#  -> Link bootfat-elf
#  -> Link bootfat-bin
#  -> Link bootiso-elf
#  -> Link bootiso-bin
# Build libkernel-sse41.a libkernel-avx libkernel-avx512
#  -> Build kernel-elf

CLANG_FLAGS = -no-integrated-as
CLANG_FLAGS += -Wno-unused-command-line-argument
#CLANG_FLAGS =
LINKER_SEL = -fuse-ld=gold

#LINKER_DEBUG = -Wl,--orphan-handling,warn
LINKER_DEBUG =

noinst_LIBRARIES = \
	libmbr.a \
	libbootsect.a \
	libkernel-sse41.a \
	libkernel-avx.a \
	libkernel-avx512.a

bin_PROGRAMS = \
	mbr-elf \
	mbr-bin \
	bootfat-elf \
	bootfat-bin \
	bootiso-elf \
	bootiso-bin \
	kernel-elf

libmbr_a_SOURCES = \
	mbr/mbr.s

libmbr_a_CCASFLAGS = \
	-m16 \
	$(CLANG_FLAGS) \
	-fno-pie -fno-pic \
	-Wno-unused-command-line-argument

EXTRA_libmbr_a_DEPENDENCIES = \
	$(abs_top_builddir)/dump

mbr_elf_LDFLAGS = \
	-m16 \
	-Wl,--oformat,elf32-little \
	-Wl,-Map,$(top_builddir)/mbr.map \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) \
	-Wl,-nostdlib \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_mbr_elf_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_elf_SOURCES = \
	mbr/mbr.ld \
	mbr/empty.cc

mbr_elf_CXXFLAGS = \
	-m16 \
	$(CLANG_FLAGS)

mbr_elf_CCASFLAGS = \
	-m16 \
	$(CLANG_FLAGS)

mbr_elf_LDADD = \
	libmbr.a

mbr_bin_SOURCES = \
	mbr/empty.cc

mbr_bin_CXXFLAGS = \
	-m16 \
	$(CLANG_FLAGS)

mbr_bin_CFLAGS = \
	-m16 \
	-fno-pic -fno-pie \
	$(CLANG_FLAGS)

mbr_bin_LDFLAGS = \
	-m16 \
	-Wl,--oformat,binary \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	-fuse-ld=bfd \
	-Wl,-nostdlib \
	-fno-pic -fno-pie \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_mbr_bin_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_bin_LDADD = libmbr.a

libbootsect_a_SOURCES = \
	boot/bootsect.ld \
	boot/bootsect.cc \
	boot/bootinfo.s \
	boot/screen.cc \
	boot/fs.cc \
	boot/fat32.cc \
	boot/iso9660.cc \
	boot/cpu.cc \
	boot/malloc.cc \
	boot/string.cc \
	boot/utf.cc \
	boot/paging.cc \
	boot/elf64.cc \
	boot/rand.cc \
	boot/physmem.cc \
	boot/farptr.cc \
	boot/vesa.cc \
	boot/progressbar.cc \
	boot/driveinfo.cc \
	boot/debug.cc \
	boot/part.s \
	boot/mpentry.s \
	boot/exception.s \
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
	boot/farptr.h \
	boot/elf64decl.h \
	boot/mpentry.h \
	boot/screen.h \
	boot/fat32.h \
	boot/driveinfo.h \
	boot/string.h \
	boot/physmem.h \
	boot/bootsect.h \
	boot/malloc.h \
	boot/fs.h \
	boot/vesa.h

libbootsect_a_INCLUDES = \
	-I$(top_srcdir)/boot

libbootsect_a_CXXFLAGS = \
	$(libbootsect_a_INCLUDES) \
	-m16 -ggdb -Oz -ffreestanding \
	-Wall -Wextra -Wpedantic \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti \
	-mrtd -mregparm=3 \
	-std=c++14 -fno-pic -fno-pie \
	$(CLANG_FLAGS)

libbootsect_a_CFLAGS = \
	-m16 -ggdb -Oz -ffreestanding \
	-Wall -Wextra -Wpedantic \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables \
	-mrtd -mregparm=3 \
	-std=c++14 -fno-pic -fno-pie \
	$(CLANG_FLAGS)

libbootsect_a_CCASFLAGS = \
	-m16 -ggdb -Oz -ffreestanding \
	-Wall -Wextra -Wpedantic \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables \
	-mrtd -mregparm=3 \
	-std=c++14 -fno-pic -fno-pie \
	$(CLANG_FLAGS)

bootfat_elf_SOURCES = \
	boot/bootfat.s

bootfat_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootfat_elf_CCASFLAGS = \
	-m16 -fno-pic -fno-pie \
	$(CLANG_FLAGS)

bootfat_elf_LDFLAGS = \
	-m16 \
	-Wl,-m,i386linux \
	-Wl,-Map,$(top_builddir)/bootsect.map \
	-Wl,-T,$(top_srcdir)/boot/bootsect.ld \
	$(LINKER_DEBUG) \
	-Wl,-v \
	$(LINKER_SEL) -fuse-ld=gold \
	-nostartfiles -nodefaultlibs -nostdlib \
	$(CLANG_FLAGS)

EXTRA_bootfat_elf_DEPENCENCIES = \
	$(top_srcdir)/boot/bootsect.ld

bootfat_elf_LDADD = \
	libbootsect.a

bootfat_bin_SOURCES = \
	boot/bootfat.s

bootfat_bin_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootfat_bin_CCASFLAGS = \
	$(bootfat_elf_CCASFLAGS)

bootfat_bin_LDFLAGS = \
	-m16 \
	-Wl,--oformat,binary \
	-Wl,-T,$(top_srcdir)/boot/bootsect.ld \
	$(LINKER_DEBUG) \
	-fuse-ld=bfd \
	-Wl,-nostdlib \
	-fno-pic -fno-pie \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_bootfat_bin_DEPENDENCIES = \
	$(top_srcdir)/boot/bootsect.ld

bootfat_bin_LDADD = \
	libbootsect.a

bootiso_elf_SOURCES = \
	boot/bootiso.s

bootiso_elf_CXXFLAGS = \
	$(bootfat_elf_CXXFLAGS)

bootiso_bin_CCASFLAGS = \
	$(bootfat_bin_CCASFLAGS)

bootiso_elf_LDFLAGS = \
	$(bootfat_elf_LDFLAGS)

bootiso_elf_LDADD = \
	libbootsect.a

bootiso_bin_SOURCES = \
	boot/bootiso.s

bootiso_bin_CXXFLAGS = \
	$(bootfat_bin_CXXFLAGS)

bootiso_bin_LDFLAGS = \
	$(bootfat_bin_LDFLAGS)

EXTRA_bootiso_bin_DEPENDENCIES = \
	$(top_srcdir)/boot/bootsect.ld

bootiso_bin_LDADD = \
	libbootsect.a

kernel_elf_SOURCES = \
	kernel/arch/x86_64/entry.s \
	kernel/arch/x86_64/cpu/isr.S \
	kernel/arch/x86_64/cpu/except_asm.s \
	kernel/arch/x86_64/cpu/syscall.s \
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
	kernel/lib/cc/algorithm.h \
	kernel/lib/cc/functional.h \
	kernel/lib/cc/initializer_list.h \
	kernel/lib/cc/memory.h \
	kernel/lib/cc/type_traits.h \
	kernel/lib/cc/utility.h \
	kernel/lib/cc/vector.h \
	kernel/lib/conio.cc \
	kernel/lib/conio.h \
	kernel/lib/debug.cc \
	kernel/lib/debug.h \
	kernel/lib/dev_eth.cc \
	kernel/lib/dev_eth.h \
	kernel/lib/dev_registration.h \
	kernel/lib/dev_storage.cc \
	kernel/lib/dev_storage.h \
	kernel/lib/dev_text.cc \
	kernel/lib/dev_text.h \
	kernel/lib/dirent.h \
	kernel/lib/errno.h \
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
	kernel/lib/rbtree.cc \
	kernel/lib/rbtree.h \
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
	kernel/lib/zlib_helper.cc \
	kernel/lib/zlib_helper.h \
	kernel/fs/ext4.cc \
	kernel/fs/fat32.cc \
	kernel/fs/fat32_decl.h \
	kernel/fs/iso9660.cc \
	kernel/fs/iso9660_decl.h \
	kernel/fs/iso9660_part.cc \
	kernel/fs/mbr.cc \
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
	kernel/arch/x86_64/bootdev.cc \
	kernel/arch/x86_64/bootdev.h \
	kernel/arch/x86_64/cpu/apic.cc \
	kernel/arch/x86_64/cpu/apic.h \
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
	kernel/arch/x86_64/cpu/ioport.cc \
	kernel/arch/x86_64/cpu/ioport.h \
	kernel/arch/x86_64/cpu/isr_constants.h \
	kernel/arch/x86_64/cpu/isr.h \
	kernel/arch/x86_64/cpu/legacy_pic.cc \
	kernel/arch/x86_64/cpu/legacy_pic.h \
	kernel/arch/x86_64/cpu/legacy_pit.cc \
	kernel/arch/x86_64/cpu/legacy_pit.h \
	kernel/arch/x86_64/cpu/math.cc \
	kernel/arch/x86_64/cpu/mmu.cc \
	kernel/arch/x86_64/cpu/nontemporal.cc \
	kernel/arch/x86_64/cpu/nontemporal.h \
	kernel/arch/x86_64/cpu/spinlock.cc \
	kernel/arch/x86_64/cpu/spinlock.h \
	kernel/arch/x86_64/cpu/syscall_dispatch.cc \
	kernel/arch/x86_64/cpu/syscall_dispatch.h \
	kernel/arch/x86_64/cpu/thread_impl.cc \
	kernel/arch/x86_64/cpu/thread_impl.h \
	kernel/arch/x86_64/device/ahci.cc \
	kernel/arch/x86_64/device/ata.cc \
	kernel/arch/x86_64/device/ata.h \
	kernel/arch/x86_64/device/ata.h \
	kernel/arch/x86_64/device/e9debug.cc \
	kernel/arch/x86_64/device/e9debug.h \
	kernel/arch/x86_64/device/e9debug.h \
	kernel/arch/x86_64/device/ide.cc \
	kernel/arch/x86_64/device/keyb8042.cc \
	kernel/arch/x86_64/device/keyb8042.h \
	kernel/arch/x86_64/device/keyb8042.h \
	kernel/arch/x86_64/device/keyb8042_layout/keyb8042_layout_us.cc \
	kernel/arch/x86_64/device/nvme.cc \
	kernel/arch/x86_64/device/nvme.h \
	kernel/arch/x86_64/device/pci.cc \
	kernel/arch/x86_64/device/pci.h \
	kernel/arch/x86_64/device/pci.h \
	kernel/arch/x86_64/device/rtl8139.cc \
	kernel/arch/x86_64/device/serial-uart.cc \
	kernel/arch/x86_64/device/serial-uart.h \
	kernel/arch/x86_64/device/serial-uart.h \
	kernel/arch/x86_64/device/usb.h \
	kernel/arch/x86_64/device/usb.h	\
	kernel/arch/x86_64/device/usb_xhci.cc \
	kernel/arch/x86_64/device/vga.cc \
	kernel/arch/x86_64/elf64.cc \
	kernel/arch/x86_64/elf64_decl.h \
	kernel/arch/x86_64/elf64.h \
	kernel/arch/x86_64/gdbstub.cc \
	kernel/arch/x86_64/gdbstub.h \
	kernel/arch/x86_64/nano_time.cc \
	kernel/arch/x86_64/stacktrace.cc \
	kernel/arch/x86_64/stacktrace.h \
	kernel/arch/x86_64/types.h

kernel_elf_INCLUDES = \
	-I$(top_srcdir)/kernel \
	-I$(top_srcdir)/kernel/lib \
	-I$(top_srcdir)/kernel/net \
	-I$(top_srcdir)/kernel/lib/cc \
	-I$(top_srcdir)/kernel/arch \
	-I$(top_srcdir)/kernel/arch/x86_64 \
	-I$(top_builddir)/generated-headers \
	-I$(EXTERNINCLUDEDIR)

kernel_elf_CXXFLAGS = \
	$(kernel_elf_INCLUDES) \
	-std=c++14 -ggdb \
	-mcmodel=kernel -fno-pic -mno-red-zone \
	-fno-stack-protector -fno-exceptions -fno-common -fno-rtti \
	-msse -msse2 -fverbose-asm

libkernel_sse41_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_sse4_1.cc

libkernel_avx_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_avx.cc

libkernel_avx512_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_avx512.cc

kernel_elf_LDFLAGS = \
	-Wl,--oformat,elf64-x86-64 \
	-Wl,-Map,$(top_builddir)/kernel.map \
	-Wl,-T,$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	$(LINKER_DEBUG) \
	-Wl,-g \
	-Wl,-export-dynamic \
	-Wl,-z,max-page-size=4096 \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_kernel_elf_DEPENDENCIES = \
	$(top_srcdir)/kernel/arch/x86_64/kernel.ld
	$(abs_top_builddir)/dump \
	$(ZLIBSTATICLIB)

libkernel_sse41_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -msse4.1
libkernel_avx_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -mavx
libkernel_avx512_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -mavx512f

kernel_elf_LDADD = \
	libkernel-sse41.a \
	libkernel-avx.a \
	libkernel-avx512.a \
	$(ZLIBSTATICLIB)

# ----------------------------------------------------------------------------
# Misc make rules

$(abs_top_builddir)/dump:
	echo WTF ******************************** $(ZLIBSTATICLIB)
	mkdir -p $(abs_top_builddir)/dump

include zlib.mk
include diskfat.mk
#include emulators.mk

# ----------------------------------------------------------------------------
# Generate register definitions

generated_headers: \
	$(top_builddir)/generated-headers/nvmebits.h \
	$(top_builddir)/generated-headers/pcibits.h

.PHONY: generated_headers

$(top_builddir)/generated-headers/nvmebits.h: \
		$(top_srcdir)/kernel/arch/x86_64/device/nvme.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

$(top_builddir)/generated-headers/pcibits.h: \
		$(top_srcdir)/kernel/arch/x86_64/device/pci.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

# ----------------------------------------------------------------------------
# QEMU launcher

dump/qemu-debug-out:
	mkfifo $@

QEMU = qemu-system-x86_64

QEMU_CPUS = 8
QEMU_CORES = 4
QEMU_THREADS = 1
QEMU_CPU = host,migratable=false,host-cache-info=true

QEMU_RAM = 5G

QEMU_USB = -device nec-usb-xhci \
	-device usb-kbd \
	-device usb-mouse

QEMU_NET = \
	-net nic,model=rtl8139 \
	-net nic,model=ne2k_pci \
	-net nic,model=e1000 \
	$(QEMU_BRIDGE) \
	-net dump,file=dump/netdump

QEMU_MONITOR = \
	-chardev socket,id=qemu-monitor,host=localhost,port=7777,server,nowait

#,telnet

QEMU_DEBUGCON = \
	-chardev pipe,path=dump/qemu-debug-out,id=qemu-debug-out \
	-mon qemu-monitor,mode=readline \
	-device isa-debugcon,chardev=qemu-debug-out

QEMU_SERIAL = \
	-chardev socket,id=qemu-serial-socket,host=localhost,port=7778,server,nowait \
	-serial chardev:qemu-serial-socket

QEMU_COMMON = \
	$(QEMU_MONITOR) \
	$(QEMU_SERIAL) \
	$(QEMU_DEBUGCON) \
	$(QEMU_USB) \
	$(QEMU_NET) \
	-no-shutdown -no-reboot -d unimp,guest_errors \
	-m $(QEMU_RAM) \
	$(QEMU_FLAGS)

QEMU_RT_debug = -s -S
QEMU_RT_run = -s
QEMU_HDCTL_DEV_nvme = \
	-machine q35 \
	-device nvme,drive=nvmedrv,serial=42424242 \
	-boot menu=on
QEMU_HDCTL_DEV_ahci = -machine q35
QEMU_HDCTL_DEV_ide = -machine pc
QEMU_DIF_nvme = ,if=none,id=nvmedrv
QEMU_DIF_ahci =
QEMU_DIF_ide =
QEMU_MEDIA_fat = disk
QEMU_MEDIA_iso = cdrom
QEMU_IMAGE_fat = fatdisk.img
QEMU_IMAGE_iso = isodisk.img
QEMU_NP_smp = -smp cpus=$(QEMU_CPUS),cores=$(QEMU_CORES),threads=$(QEMU_THREADS)
QEMU_NP_up = -smp cpus=1,cores=1,threads=1
QEMU_EMU_kvm = -enable-kvm -cpu $(QEMU_CPU)
QEMU_EMU_tcg = -cpu kvm64

#QEMU_BIOS = -bios $(top_srcdir)/qemu-q35-20170927.rom
QEMU_BIOS = -bios $(top_srcdir)/qemu-seabios.rom

# Connect to debug log
monitor-debug-output:
	while true; do cat dump/qemu-debug-out; done

# Connect to console
monitor-connect:
	telnet localhost 7777

# Generate emulators makefile
qemu-emu.mk: $(top_srcdir)/Makefile.am
	@echo Generating qemu-emu.mk
	@rm -f qemu-emu.mk
	@for rt in debug run; do \
		for np in up smp; do \
			for fs in iso fat; do \
				for sc in ahci ide nvme; do \
					for em in kvm tcg; do \
						IMG="\$$(QEMU_IMAGE_$$fs)"; \
						QUP="\$$(QEMU_NP_$$np)"; \
						CTL="\$$(QEMU_HDCTL_DEV_$$sc)"; \
						DIF="\$$(QEMU_DIF_$$sc)"; \
						QM="\$$(QEMU_MEDIA_$$fs)"; \
						QEM="\$$(QEMU_EMU_$$em)"; \
						QRT="\$$(QEMU_RT_$$rt)"; \
						printf %s \
							"$$rt-$$np-$$fs-$$sc-$$em: " \
							$$IMG \
							' dump/qemu-debug-out' \
							$$'\n' $$'\t' \
							'$$(QEMU) $$(QEMU_COMMON)' \
							' \' $$'\n' $$'\t' \
							$$QUP \
							' \' $$'\n' $$'\t' \
							$$CTL \
							' \' $$'\n' $$'\t' \
							$$QEM \
							' \' $$'\n' $$'\t' \
							'-drive' \
							" file=$$IMG$$DIF,format=raw,media=$$QM" \
							' \' $$'\n' $$'\t' \
							$$QRT \
							' \' $$'\n' $$'\t' \
							'\$$(QEMU_BIOS)' \
							$$'\n' $$'\n' \
							>> qemu-emu.mk; \
					done; \
				done; \
			done; \
		done; \
	done

-include qemu-emu.mk
