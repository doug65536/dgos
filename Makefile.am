## Process this file with automake to produce Makefile.in

bin_PROGRAMS = \
	bootsect-elf \
	kernel-elf

noinst_LIBRARIES = \
	libkernel-sse41.a \
	libkernel-avx.a \
	libkernel-avx512.a

bootsect_elf_SOURCES = \
	boot/bootsect.cc \
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
	boot/exception.s

bootsect_elf_CXXFLAGS = \
	-I$(top_srcdir)/boot \
	-m16 -ggdb -Os -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti \
	-mrtd -mregparm=3 \
	-std=c++14 -fno-pic -fno-pie

bootsect_elf_LDFLAGS = \
	-Wl,-m -Wl,i386linux \
	-Wl,-nostdlib \
	-Wl,-M \
	-Wl,-T -Wl,$(top_srcdir)/boot/bootsect.ld \
	-no-integrated-as \
	-Wl,-v \
	-fuse-ld=gold \
	-nostartfiles -nodefaultlibs -nostdlib

bootsect_elf_CCASFLAGS = \
	-m16

kernel_elf_SOURCES = \
	kernel/arch/x86_64/entry.s \
	kernel/arch/x86_64/cpu/isr.S \
	kernel/arch/x86_64/cpu/except_asm.s \
	kernel/arch/x86_64/cpu/syscall.s \
	kernel/main.cc \
	kernel/lib/mm.cc \
	kernel/lib/thread.cc \
	kernel/lib/priorityqueue.cc \
	kernel/lib/threadsync.cc \
	kernel/lib/callout.cc \
	kernel/lib/printk.cc \
	kernel/lib/math.cc \
	kernel/lib/dev_text.cc \
	kernel/lib/dev_storage.cc \
	kernel/lib/dev_eth.cc \
	kernel/lib/conio.cc \
	kernel/lib/mouse.cc \
	kernel/lib/keyboard.cc \
	kernel/lib/string.cc \
	kernel/lib/irq.cc \
	kernel/lib/time.cc \
	kernel/lib/stdlib.cc \
	kernel/lib/assert.cc \
	kernel/lib/bitsearch.cc \
	kernel/lib/bsearch.cc \
	kernel/lib/rbtree.cc \
	kernel/lib/debug.cc \
	kernel/lib/rand.cc \
	kernel/lib/bswap.cc \
	kernel/lib/pool.cc \
	kernel/lib/heap.cc \
	kernel/lib/hash.cc \
	kernel/lib/hash_table.cc \
	kernel/lib/fileio.cc \
	kernel/lib/process.cc \
	kernel/lib/png.cc \
	kernel/lib/zlib_helper.cc \
	kernel/lib/framebuffer.cc \
	kernel/lib/unique_ptr.cc \
	kernel/net/eth_q.cc \
	kernel/net/eth_frame.cc \
	kernel/net/arp_frame.cc \
	kernel/net/ipv4_frame.cc \
	kernel/net/tcp_frame.cc \
	kernel/net/udp_frame.cc \
	kernel/net/icmp_frame.cc \
	kernel/net/icmp.cc \
	kernel/net/ipv4.cc \
	kernel/net/udp.cc \
	kernel/net/tcp.cc \
	kernel/net/dhcp.cc \
	kernel/arch/x86_64/cpu/syscall_dispatch.cc \
	kernel/arch/x86_64/bios_data.cc \
	kernel/arch/x86_64/nano_time.cc \
	kernel/arch/x86_64/cpu/cpu.cc \
	kernel/arch/x86_64/cpu/apic.cc \
	kernel/arch/x86_64/cpu/cpu_broadcast.cc \
	kernel/arch/x86_64/cpu/cmos.cc \
	kernel/arch/x86_64/cpu/atomic.cc \
	kernel/arch/x86_64/cpu/control_regs.cc \
	kernel/arch/x86_64/cpu/cpuid.cc \
	kernel/arch/x86_64/cpu/thread_impl.cc \
	kernel/arch/x86_64/cpu/spinlock.cc \
	kernel/arch/x86_64/cpu/halt.cc \
	kernel/arch/x86_64/cpu/gdt.cc \
	kernel/arch/x86_64/cpu/idt.cc \
	kernel/arch/x86_64/cpu/interrupts.cc \
	kernel/arch/x86_64/cpu/ioport.cc \
	kernel/arch/x86_64/cpu/mmu.cc \
	kernel/arch/x86_64/cpu/nontemporal.cc \
	kernel/arch/x86_64/cpu/legacy_pic.cc \
	kernel/arch/x86_64/cpu/legacy_pit.cc \
	kernel/arch/x86_64/cpu/except.cc \
	kernel/arch/x86_64/cpu/math.cc \
	kernel/arch/x86_64/device/pci.cc \
	kernel/arch/x86_64/device/keyb8042.cc \
	kernel/arch/x86_64/device/e9debug.cc \
	kernel/arch/x86_64/device/rtl8139.cc \
	kernel/arch/x86_64/device/keyb8042_layout/keyb8042_layout_us.cc \
	kernel/arch/x86_64/device/usb_xhci.cc \
	kernel/arch/x86_64/bootdev.cc \
	kernel/arch/x86_64/elf64.cc \
	kernel/arch/x86_64/stacktrace.cc \
	kernel/arch/x86_64/gdbstub.cc \
	kernel/arch/x86_64/device/vga.cc \
	kernel/arch/x86_64/device/ahci.cc \
	kernel/arch/x86_64/device/ide.cc \
	kernel/arch/x86_64/device/ata.cc \
	kernel/arch/x86_64/device/serial-uart.cc

kernel_elf_CXXFLAGS = \
	-I$(top_srcdir)/kernel \
	-I$(top_srcdir)/kernel/lib \
	-I$(top_srcdir)/kernel/net \
	-I$(top_srcdir)/kernel/lib/cc \
	-I$(top_srcdir)/kernel/arch \
	-I$(top_srcdir)/kernel/arch/x86_64 \
	-std=c++14 -ggdb \
	-mcmodel=kernel -fno-pie -mno-red-zone \
	-fvisibility=hidden \
	-fno-stack-protector -fno-exceptions -fno-common -fno-rtti \
	-msse -msse2 -fverbose-asm

libkernel_sse41_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_sse4_1.cc

libkernel_avx_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_avx.cc

libkernel_avx512_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_avx512.cc

kernel_elf_LDFLAGS = \
	-Wl,--oformat -Wl,elf64-x86-64 \
	-Wl,-Map -Wl,$(top_builddir)/dump/kernel.map \
	-Wl,-T -Wl,$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	-Wl,-g -Wl,-export-dynamic \
	-Wl,-z -Wl,max-page-size=4096 \
	-nostartfiles -nodefaultlibs -nostdlib

libkernel_sse41_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -msse4.1
libkernel_avx_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -mavx
libkernel_avx512_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -mavx512f

kernel_elf_LDADD = \
	libkernel-sse41.a \
	libkernel-avx.a \
	libkernel-avx512.a

# ----------------------------------------------------------------------------
# Misc make rules

$(top_builddir)/dump:
	mkdir -p $(top_builddir)/dump

# ----------------------------------------------------------------------------
# ZLib

EXTERNDIR = $(top_builddir)/extern
ZLIBVER = 1.2.11
ZLIBBUILDROOT = $(EXTERNDIR)/zlib-src
ZLIBSOURCEDIR = $(ZLIBBUILDROOT)/zlib-$(ZLIBVER)
ZLIBGZFILE = zlib-$(ZLIBVER).tar.gz
ZLIBURL = http://zlib.net/$(ZLIBGZFILE)
ZLIBCFLAGS = -O3 -mcmodel=kernel -fno-pie \
	-mno-red-zone -msse -msse2 -fno-exceptions \
	-fno-common -fno-stack-protector -fbuiltin

EXTERNINCLUDEDIR = $(EXTERNDIR)/include
ZLIBINCLUDEDIR = $(EXTERNINCLUDEDIR)/zlib
ZLIBHEADERS = zlib.h zconf.h

#ZLIBHEADERSOURCES = $(patsubst %,$(ZLIBSOURCEDIR)/%,$(ZLIBHEADERS))
ZLIBHEADERSOURCES = $(ZLIBSOURCEDIR)/zlib.h $(ZLIBSOURCEDIR)/zconf.h

#ZLIBPUBLICINCLUDES = $(patsubst %,$(ZLIBINCLUDEDIR)/%,$(ZLIBHEADERS))
ZLIBPUBLICINCLUDES = $(ZLIBINCLUDEDIR)/zlib.h $(ZLIBINCLUDEDIR)/zconf.h

# -fno-asynchronous-unwind-tables

ZLIBCONFIGURE = $(ZLIBSOURCEDIR)/configure
ZLIBLIBDIR = $(EXTERNDIR)/zlib-src/build
ZLIBLIBNAME = libz.a
ZLIBSTATICLIB = $(ZLIBLIBDIR)/$(ZLIBLIBNAME)
ZLIBCONFIGUREFLAGS = --64 --static
ZLIBMAKEFILE = $(ZLIBLIBDIR)/Makefile
ZLIBMAKE = $(MAKE)

# Make zlib
$(ZLIBSTATICLIB) $(ZLIBPUBLICINCLUDES): $(ZLIBMAKEFILE)
	( cd $(@D) && $(ZLIBMAKE) $(ZLIBLIBNAME) )
	mkdir -p $(ZLIBINCLUDEDIR)
	for i in $(ZLIBHEADERS); do \
		cp $(ZLIBSOURCEDIR)/$$i $(ZLIBINCLUDEDIR)/$$i;\
	done

# Run zlib configure
$(ZLIBMAKEFILE): $(ZLIBCONFIGURE)
	( \
		mkdir -p $(@D) && \
		cd $(@D) && \
		CFLAGS="$(ZLIBCFLAGS)" ../zlib-$(ZLIBVER)/configure $(ZLIBCONFIGUREFLAGS) \
	)

# Download zlib
$(ZLIBCONFIGURE):
	mkdir -p $(ZLIBBUILDROOT)
	curl -o $(ZLIBBUILDROOT)/zlib.tar.gz $(ZLIBURL)
	(cd $(ZLIBBUILDROOT) && tar -xzvf zlib.tar.gz)

BUILT_SOURCES = $(ZLIBSTATICLIB) $(ZLIBPUBLICINCLUDES) $(top_builddir)/dump

kernel_elf_LDADD += $(ZLIBSTATICLIB)
kernel_elf_CXXFLAGS += -I$(EXTERNINCLUDEDIR)
