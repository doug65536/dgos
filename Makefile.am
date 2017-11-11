## Process this file with automake to produce Makefile.in

BUILT_SOURCES = \
	$(ZLIBSTATICLIB) \
	$(ZLIBPUBLICINCLUDES) \
	$(top_builddir)/qemu-emu.mk \
	$(abs_top_builddir)/dump \
	$(top_builddir)/dump/qemu-debug-out \
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

#LINKER_SEL = -fuse-ld=gold -z max-page-size=256
LINKER_SEL =

#LINKER_DEBUG = -Wl,--orphan-handling,warn
LINKER_DEBUG = 

noinst_LIBRARIES = \
	libmbr.a \
	libbootsect.a \
	libkernel-sse41.a \
	libkernel-avx.a \
	libkernel-avx512.a \
    libc.a

bin_PROGRAMS = \
	mbr-elf \
	mbr-bin \
	bootfat-elf \
	bootfat-bin \
	bootiso-elf \
	bootiso-bin \
	kernel-elf
	
#user-hello

# libmbr.a

libmbr_a_SOURCES = \
	mbr/mbr.s

libmbr_a_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(COMPILER_FLAGS)

EXTRA_libmbr_a_DEPENDENCIES = \
	$(abs_top_builddir)/dump

# mbr-elf

mbr_elf_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	-Wl,-melf_i386 \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) -fuse-ld=bfd -static \
	-Wl,-nostdlib \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_mbr_elf_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_elf_SOURCES = \
	mbr/mbr.ld \
	mbr/empty.cc

mbr_elf_CXXFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(COMPILER_FLAGS)

mbr_elf_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

mbr_elf_LDADD = \
	libmbr.a

# mbr-bin

mbr_bin_SOURCES = \
	mbr/empty.cc

mbr_bin_CXXFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

mbr_bin_CFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

mbr_bin_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	-Wl,-melf_i386 \
	-Wl,--oformat,binary \
	-Wl,-Map,$(top_builddir)/mbr.map \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) -fuse-ld=bfd -static \
	-Wl,-nostdlib \
	$(NO_PIC_FLAGS) \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_mbr_bin_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_bin_LDADD = libmbr.a

# libbootsect.a

libbootsect_a_SOURCES = \
	boot/bootcommon.s \
	boot/bioscall.S \
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
	$(REAL_MODE_FLAGS) \
	-m32 \
	$(libbootsect_a_INCLUDES) \
	$(SMALLEST_CODE_FLAGS) \
	-ffreestanding \
	$(NO_STACKPROTECTOR_FLAGS) $(NO_EXCEPTIONS_FLAGS) -fno-rtti \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(NO_COMMON_FLAGS) \
	-mrtd -mregparm=3 \
	-std=c++14 $(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

libbootsect_a_CFLAGS = \
	-m32 \
	$(REAL_MODE_FLAGS) \
	$(libbootsect_a_INCLUDES) \
	$(SMALLEST_CODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	-ffreestanding \
	$(NO_STACKPROTECTOR_FLAGS) $(NO_COMMON_FLAGS) -fno-rtti \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(NO_COMMON_FLAGS) \
	-std=c++14 \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)


libbootsect_a_CCASFLAGS = \
	-m32 \
	$(SMALLEST_CODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	-ffreestanding \
	$(NO_UNWIND_TABLES_FLAGS) \
	-std=c++14 \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

# bootfat-elf

bootfat_elf_SOURCES = \
	boot/bootfat.s

bootfat_elf_CXXFLAGS = \
	-m32 \
	$(REAL_MODE_FLAGS) \
	$(libbootsect_a_CXXFLAGS)


bootfat_elf_CCASFLAGS = \
	-m32 \
	$(REAL_MODE_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)


bootfat_elf_LDFLAGS = \
	-m32 \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-melf_i386 \
	-Wl,-Map,$(top_builddir)/bootfat.map \
	-Wl,-T,$(top_srcdir)/boot/bootfat.ld \
	$(LINKER_SEL) -static \
	-nostartfiles -nodefaultlibs -nostdlib \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

EXTRA_bootfat_elf_DEPENCENCIES = \
	$(top_srcdir)/boot/bootfat.ld

bootfat_elf_LDADD = \
	libbootsect.a

# bootfat-bin

bootfat_bin_SOURCES = \
	$(bootfat_elf_SOURCES)

bootfat_bin_CXXFLAGS = \
	-m32 \
	$(libbootsect_a_CXXFLAGS)

bootfat_bin_CCASFLAGS = \
	-m32 \
	$(REAL_MODE_FLAGS) \
	$(bootfat_elf_CCASFLAGS)


bootfat_bin_LDFLAGS = \
	-m32 \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-melf_i386 \
	-Wl,--oformat,binary \
	-Wl,-T,$(top_srcdir)/boot/bootfat.ld \
	$(LINKER_SEL) -fuse-ld=bfd -static \
	-Wl,-nostdlib \
	-nostartfiles -nodefaultlibs -nostdlib -static \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

EXTRA_bootfat_bin_DEPENDENCIES = \
	$(top_srcdir)/boot/bootfat.ld

bootfat_bin_LDADD = \
	libbootsect.a

# bootiso-elf

bootiso_elf_SOURCES = \
	boot/bootiso.s

bootiso_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)


bootiso_elf_CCASFLAGS = \
	-m32 \
	-Wa,-g \
	$(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

# $(REAL_MODE_FLAGS)

bootiso_elf_LDFLAGS = \
	-m32 \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-melf_i386 \
	-Wl,-T,$(top_srcdir)/boot/bootiso.ld \
	$(LINKER_SEL) -static \
	-nostartfiles -nodefaultlibs -nostdlib \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

EXTRA_bootiso_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/bootiso.ld

bootiso_elf_LDADD = \
	libbootsect.a

# bootiso-bin

bootiso_bin_SOURCES = \
	$(bootiso_elf_SOURCES)

bootiso_bin_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootiso_bin_CCASFLAGS = \
	-m32 \
	-Wa,-g \
	$(bootiso_elf_CCASFLAGS)

# $(REAL_MODE_FLAGS)

bootiso_bin_LDFLAGS = \
	-m32 \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-m,elf_i386 \
	-Wl,--oformat,binary \
	-Wl,-Map,$(top_builddir)/bootiso.map \
	-Wl,-T,$(top_srcdir)/boot/bootiso.ld \
	-fuse-ld=bfd -static \
	-nostartfiles -nodefaultlibs -nostdlib \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

EXTRA_bootiso_bin_DEPENDENCIES = \
	$(top_srcdir)/boot/bootiso.ld

bootiso_bin_LDADD = \
	libbootsect.a

# kernel-elf

kernel_elf_SOURCES = \
	kernel/arch/x86_64/entry.s \
	kernel/arch/x86_64/cpu/isr.S \
	kernel/arch/x86_64/cpu/except_asm.s \
	kernel/arch/x86_64/cpu/syscall.S \
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
	kernel/lib/desc_alloc.cc \
	kernel/lib/desc_alloc.h \
	kernel/lib/dev_eth.cc \
	kernel/lib/dev_eth.h \
	kernel/lib/dev_registration.h \
	kernel/lib/dev_storage.cc \
	kernel/lib/dev_storage.h \
	kernel/lib/dev_text.cc \
	kernel/lib/dev_text.h \
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
	kernel/lib/mutex.cc \
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
	kernel/lib/work_queue.cc \
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
	kernel/arch/x86_64/cpu/syscall_dispatch.cc \
	kernel/arch/x86_64/cpu/syscall_dispatch.h \
	kernel/arch/x86_64/cpu/thread_impl.cc \
	kernel/arch/x86_64/cpu/thread_impl.h \
	kernel/device/ahci.cc \
	kernel/device/ata.cc \
	kernel/device/ata.h \
	kernel/device/ata.h \
	kernel/device/e9debug.cc \
	kernel/device/e9debug.h \
	kernel/device/e9debug.h \
	kernel/device/ide.cc \
	kernel/device/keyb8042.cc \
	kernel/device/keyb8042.h \
	kernel/device/keyb8042.h \
	kernel/device/keyb8042_layout/keyb8042_layout_us.cc \
	kernel/device/nvme.cc \
	kernel/device/nvme.h \
	kernel/device/pci.cc \
	kernel/device/pci.h \
	kernel/device/pci.h \
	kernel/device/rtl8139.cc \
	kernel/device/serial-uart.cc \
	kernel/device/serial-uart.h \
	kernel/device/serial-uart.h \
	kernel/device/usb.h \
	kernel/device/usb.h	\
	kernel/device/usb_xhci.cc \
	kernel/device/vga.cc \
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
	kernel/syscall/fd.cc \
	kernel/syscall/mem.cc

kernel_elf_INCLUDES = \
	-I$(top_srcdir)/kernel \
	-I$(top_srcdir)/kernel/lib \
	-I$(top_srcdir)/kernel/net \
	-I$(top_srcdir)/kernel/lib/cc \
	-I$(top_srcdir)/kernel/arch \
	-I$(top_srcdir)/kernel/arch/x86_64 \
	-I$(EXTERNINCLUDEDIR)

kernel_elf_CXXFLAGS = \
    -D__DGOS_KERNEL__=0x00000001 \
	$(kernel_elf_INCLUDES) \
	-std=c++14 \
	-mcmodel=kernel $(NO_PIC_FLAGS) $(NO_REDZONE_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	$(NO_COMMON_FLAGS) \
	-fno-rtti \
	-msse -msse2 -fverbose-asm \
	-fvisibility=default \
	-export-dynamic \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS)

kernel_elf_CCASFLAGS = \
	$(COMPILER_FLAGS)

libkernel_sse41_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_sse4_1.cc

libkernel_avx_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_avx.cc

libkernel_avx512_a_SOURCES = \
	kernel/arch/x86_64/cpu/nontemporal_avx512.cc

kernel_elf_LDFLAGS = \
	-Wl,-melf_x86_64 \
	-Wl,-Map,$(top_builddir)/kernel.map \
	-Wl,-T,$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	$(LINKER_DEBUG) \
	-Wl,-g \
	-export-dynamic \
	-Wl,-z,max-page-size=4096 \
	-nostartfiles -nodefaultlibs -nostdlib

#-Wl,-export-dynamic

EXTRA_kernel_elf_DEPENDENCIES = \
	$(top_srcdir)/kernel/arch/x86_64/kernel.ld
	$(abs_top_builddir)/dump \
	$(ZLIBSTATICLIB) \
	$(ZLIBPUBLICINCLUDES)

libkernel_sse41_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -msse4.1
libkernel_avx_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -mavx
libkernel_avx512_a_CXXFLAGS = $(kernel_elf_CXXFLAGS) -mavx512f

kernel_elf_LDADD = \
	libkernel-sse41.a \
	libkernel-avx.a \
	libkernel-avx512.a \
	$(ZLIBSTATICLIB)

# ----------------------------------------------------------------------------
# libc

libc_a_SOURCES = \
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
    libc/src/stdlib/_Exit.cc \
    libc/src/sys/syscall.s \
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
    libc/include/setjmp.h \
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
    libc/include/locale.h

libc_a_CXXFLAGS = \
    -isystem $(top_srcdir)/libc/include \
    -isystem $(top_builddir)/cross-tool/x86_64-elf/include \
    -ffreestanding \
    $(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS)    

# ----------------------------------------------------------------------------
# libc unit tests

LIBC_TESTS = \
	$(top_srcdir)/libc/tests/testassert.cc \
	$(top_srcdir)/libc/tests/test_string.cc

LIBC_CXXFLAGS = \
	-ggdb \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	-fno-exceptions \
	-ffreestanding \
	-isystem $(top_srcdir)/libc/include \
	-I$(top_builddir) \
	-L$(top_builddir)

$(top_builddir)/libc_tests: $(LIBC_TESTS) $(top_builddir)/libc.a
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
		-static -nostdlib \
		-v \
		$(top_builddir)/libc.a -lgcc

test_compile: $(top_builddir)/libc_tests

test: test_compile
	$(top_builddir)/libc_tests

.PHONY: test_compile test

# ----------------------------------------------------------------------------
# User programs

#user_hello_SOURCES = 

# ----------------------------------------------------------------------------
# Misc make rules

$(abs_top_builddir)/dump:
	echo WTF ******************************** $(ZLIBSTATICLIB)
	mkdir -p $(abs_top_builddir)/dump

include zlib.mk
include symbols.mk
include diskfat.mk
include diskiso.mk

clean-local:
	rm -f $(top_builddir)/mbr.sym
	rm -f $(top_builddir)/mbr.map
	rm -f $(top_builddir)/kernel.sym
	rm -f $(top_builddir)/kernel.map
	rm -f $(top_builddir)/bootfat.sym
	rm -f $(top_builddir)/bootfat.map
	rm -f $(top_builddir)/bootiso.sym
	rm -f $(top_builddir)/bootiso.map

distclean-local:
	rm -rf $(top_builddir)/extern
	rm -f $(top_builddir)/fatdisk.img
	rm -f $(top_builddir)/fatpart.img
	rm -f $(top_builddir)/qemu-emu.mk
	rm -rf $(top_builddir)/dump
	rm -rf $(MUSL_BUILDDIR)
	rm -rf $(MUSL_INSTDIR)

# ----------------------------------------------------------------------------
# Generate register definitions

generated_headers: \
	$(top_srcdir)/kernel/device/nvmebits.h \
	$(top_srcdir)/kernel/device/pcibits.h

.PHONY: generated_headers

$(top_srcdir)/kernel/device/nvmebits.h: \
		$(top_srcdir)/kernel/device/nvme.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

$(top_srcdir)/kernel/device/pcibits.h: \
		$(top_srcdir)/kernel/device/pci.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

# ----------------------------------------------------------------------------
# Build musl

MUSL_BUILDDIR = $(abs_top_builddir)/musl-build
MUSL_INSTDIR = $(abs_top_builddir)/musl-install
MUSL_CFLAGS = $(NO_REDZONE_FLAGS) $(NO_UNWIND_TABLES_FLAGS) -mtune=znver1
MUSL_ARCH = x86_64

MUSL_SRCDIR = $(abs_top_srcdir)/musl

MUSL_BUILDOUT = \
	$(MUSL_BUILDDIR)/lib/libxnet.a \
	$(MUSL_BUILDDIR)/lib/libutil.a \
	$(MUSL_BUILDDIR)/lib/libc.a \
	$(MUSL_BUILDDIR)/lib/libm.a \
	$(MUSL_BUILDDIR)/lib/librt.a \
	$(MUSL_BUILDDIR)/lib/libdl.a \
	$(MUSL_BUILDDIR)/lib/libresolv.a \
	$(MUSL_BUILDDIR)/lib/libpthread.a \
	$(MUSL_BUILDDIR)/lib/libcrypt.a

MUSL_INSTOUT = \
	$(MUSL_INSTDIR)/lib/libxnet.a \
	$(MUSL_INSTDIR)/lib/libutil.a \
	$(MUSL_INSTDIR)/lib/libc.a \
	$(MUSL_INSTDIR)/lib/libm.a \
	$(MUSL_INSTDIR)/lib/librt.a \
	$(MUSL_INSTDIR)/lib/libdl.a \
	$(MUSL_INSTDIR)/lib/libresolv.a \
	$(MUSL_INSTDIR)/lib/libpthread.a \
	$(MUSL_INSTDIR)/lib/libcrypt.a

$(MUSL_BUILDDIR)/Makefile: $(MUSL_SRCDIR)/configure
	( \
		muslpath=$$(readlink -f "$(top_srcdir)/musl") && \
		mkdir -p $(MUSL_BUILDDIR) && \
		cd $(MUSL_BUILDDIR) && \
		CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) \
		CFLAGS="$(MUSL_CFLAGS)" $$muslpath/configure \
			--prefix=$(MUSL_INSTDIR) \
	)

$(MUSL_BUILDOUT): $(MUSL_BUILDDIR)/Makefile
	( \
		cd $(MUSL_BUILDDIR) && \
			make ARCH=$(MUSL_ARCH) \
	)

$(MUSL_INSTOUT): $(MUSL_BUILDOUT)
	( \
		cd $(MUSL_BUILDDIR) && \
			make install \
	)

musl: $(MUSL_INSTOUT)

.PHONY: musl

# ----------------------------------------------------------------------------
# QEMU launcher

$(top_builddir)/dump/qemu-debug-out:
	mkdir -p "$(@D)"
	mkfifo "$@"

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
	-chardev pipe,path=$(top_builddir)/dump/qemu-debug-out,id=qemu-debug-out \
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
	while true; do cat "$(top_builddir)/dump/qemu-debug-out"; done

# Connect to console
monitor-connect:
	telnet localhost 7777

# Generate emulators makefile
$(top_builddir)/qemu-emu.mk: $(top_srcdir)/Makefile.am $(top_srcdir)/qemu-emu-generator
	$(top_srcdir)/qemu-emu-generator

-include qemu-emu.mk
