## Process this file with automake to produce Makefile.in

DUMPDIR = $(top_builddir)/dump

BUILT_SOURCES = \
	$(top_builddir)/qemu-emu.mk \
	$(DUMPDIR)/qemu-debug-out \
	generated_headers

# Build libmbr
#  -> Link mbr-elf
#  -> Link mbr-bin
# Build libbootsect
#  -> Link bootfat-elf
#  -> Link bootfat-bin
#  -> Link bootiso-elf
#  -> Link bootiso-bin
# Build libkernel-{generic,bmi,avx512}
#  -> Build kernel-*

#LINKER_SEL = -fuse-ld=gold -z max-page-size=256
LINKER_SEL =

#LINKER_DEBUG = -Wl,--orphan-handling,warn
LINKER_DEBUG =

noinst_LIBRARIES = \
	libmbr.a \
	libbootsect.a \
	libc.a

bin_PROGRAMS = \
	mbr-elf \
	mbr-bin \
	bootfat-elf \
	bootfat-bin \
	bootiso-elf \
	bootiso-bin \
	kernel-generic \
	kernel-bmi \
	user-shell

KERNEL_BMI_FLAGS = \
	$(BMI_FLAGS)

# libmbr.a

libmbr_a_SOURCES = \
	mbr/mbr.s

libmbr_a_CFLAGS = \
	$(REAL_MODE_FLAGS)

libmbr_a_CXXFLAGS = \
	$(REAL_MODE_FLAGS)

libmbr_a_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(COMPILER_FLAGS) -static

EXTRA_libmbr_a_DEPENDENCIES = \
	$(DUMPDIR)

# mbr-elf

mbr_elf_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	-Wl,-melf_i386 \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) -fuse-ld=bfd -static \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_mbr_elf_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_elf_SOURCES = \
	mbr/mbr.ld \
	mbr/empty.cc

mbr_elf_CXXFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(COMPILER_FLAGS) \
	$(NO_PIC_FLAGS)

mbr_elf_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

mbr_elf_LDADD = \
	libmbr.a $(GCC_REALMODE_LIB)

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
	$(REGPARM_FLAGS) \
	-Wl,-melf_i386 \
	-Wl,--oformat,binary \
	-Wl,-Map,$(top_builddir)/mbr.map \
	-Wl,-T,$(top_srcdir)/mbr/mbr.ld \
	$(LINKER_DEBUG) \
	$(LINKER_SEL) -fuse-ld=bfd -static \
	$(NO_PIC_FLAGS) \
	-nostartfiles -nodefaultlibs -nostdlib

EXTRA_mbr_bin_DEPENDENCIES = \
	$(top_srcdir)/mbr/mbr.ld

mbr_bin_LDADD = \
	libmbr.a $(GCC_REALMODE_LIB)

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
	boot/mpentry.S \
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
	boot/vesa.h \
	boot/tui.h \
	boot/tui.cc

libbootsect_a_INCLUDES = \
	-I$(top_srcdir)/boot

libbootsect_a_CXXFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(libbootsect_a_INCLUDES) \
	$(SMALLEST_CODE_FLAGS) \
	$(NO_PIC_FLAGS) -static \
	$(FREESTANDING_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(NO_COMMON_FLAGS) \
	-mrtd -mregparm=3 \
	-std=c++14 $(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(NO_STACK_CHECK_FLAGS) \
	$(COMPILER_FLAGS)

libbootsect_a_CFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(libbootsect_a_INCLUDES) \
	$(SMALLEST_CODE_FLAGS) \
	$(NO_PIC_FLAGS) -static \
	$(FREESTANDING_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_STACK_CHECK_FLAGS) \
	$(NO_COMMON_FLAGS) \
	$(NO_RTTI_FLAGS) \
	$(NO_UNWIND_TABLES_FLAGS) \
	$(NO_COMMON_FLAGS) \
	-std=c++14 \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

libbootsect_a_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(SMALLEST_CODE_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(FREESTANDING_FLAGS) \
	$(NO_UNWIND_TABLES_FLAGS) \
	-std=c++14 \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

# bootfat-elf

bootfat_elf_SOURCES = \
	boot/bootfat.s

bootfat_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootfat_elf_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) -static \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

bootfat_elf_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(LINKER_DEBUG) \
	-Wl,-melf_i386 \
	-Wl,-Map,$(top_builddir)/bootfat.map \
	-Wl,-T,$(top_srcdir)/boot/bootfat.ld \
	$(LINKER_SEL) -static \
	-nostartfiles -nodefaultlibs -nostdlib \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

EXTRA_bootfat_elf_DEPENDENCIES = \
	$(top_srcdir)/boot/bootfat.ld

bootfat_elf_LDADD = \
	libbootsect.a $(GCC_REALMODE_LIB)

# bootfat-bin

bootfat_bin_SOURCES = \
	$(bootfat_elf_SOURCES)

bootfat_bin_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootfat_bin_CCASFLAGS = \
	$(bootfat_elf_CCASFLAGS)

bootfat_bin_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
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
	libbootsect.a $(GCC_REALMODE_LIB)

# bootiso-elf

bootiso_elf_SOURCES = \
	boot/bootiso.s

bootiso_elf_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootiso_elf_CCASFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(NO_PIC_FLAGS) \
	$(OPTIMIZE_SIZE_FLAGS) \
	$(COMPILER_FLAGS)

# $(REAL_MODE_FLAGS)

bootiso_elf_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
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
	libbootsect.a $(GCC_REALMODE_LIB)

# bootiso-bin

bootiso_bin_SOURCES = \
	$(bootiso_elf_SOURCES)

bootiso_bin_CXXFLAGS = \
	$(libbootsect_a_CXXFLAGS)

bootiso_bin_CCASFLAGS = \
	$(bootiso_elf_CCASFLAGS)

# $(REAL_MODE_FLAGS)

bootiso_bin_LDFLAGS = \
	$(REAL_MODE_FLAGS) \
	$(REGPARM_FLAGS) \
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
	libbootsect.a $(GCC_REALMODE_LIB)

# kernel

KERNEL_SOURCES_SHARED = \
	kernel/arch/x86_64/entry.S \
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
	kernel/lib/cc/numeric.h \
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
	kernel/device/iocp.cc \
	kernel/device/iocp.h \
	kernel/device/ahci.cc \
	kernel/device/ata.cc \
	kernel/device/ata.h \
	kernel/device/ata.h \
	kernel/device/e9debug.cc \
	kernel/device/e9debug.h \
	kernel/device/e9debug.h \
	kernel/device/i350.cc \
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
	kernel/device/usb.h	\
	kernel/device/usb.cc \
	kernel/device/dev_usb_ctl.cc \
	kernel/device/usb_hid.h \
	kernel/device/usb_hid.cc \
	kernel/device/usb_hub.h \
	kernel/device/usb_hub.cc \
	kernel/device/usb_storage.h \
	kernel/device/usb_storage.cc \
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
	kernel/syscall/sys_fd.cc \
	kernel/syscall/sys_mem.cc \
	kernel/syscall/sys_time.cc \
	kernel/syscall/sys_process.cc

KERNEL_INCLUDES_SHARED = \
	-I$(top_srcdir)/kernel \
	-I$(top_srcdir)/kernel/lib \
	-I$(top_srcdir)/kernel/net \
	-I$(top_srcdir)/kernel/lib/cc \
	-I$(top_srcdir)/kernel/arch \
	-I$(top_srcdir)/kernel/arch/x86_64

KERNEL_CXXFLAGS_SHARED = \
	-D__DGOS_KERNEL__=0x00000001 \
	-std=c++14 \
	-mcmodel=kernel $(NO_PIC_FLAGS) $(NO_REDZONE_FLAGS) \
	$(STACK_CHECK_FLAGS) \
	$(NO_STACKPROTECTOR_FLAGS) \
	$(NO_EXCEPTIONS_FLAGS) \
	$(NO_COMMON_FLAGS) \
	$(NO_RTTI_FLAGS) \
	-fvisibility=default \
	-export-dynamic \
	-mgeneral-regs-only \
	$(FREESTANDING_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(COMPILER_FLAGS)

KERNEL_CCASFLAGS_SHARED = \
	$(COMPILER_FLAGS)

KERNEL_LDFLAGS_SHARED = \
	-Wl,-melf_x86_64 \
	-Wl,-Map,$(top_builddir)/kernel.map \
	-Wl,-T,$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	$(LINKER_DEBUG) \
	-Wl,-g \
	-export-dynamic \
	-Wl,-z,max-page-size=4096 \
	-nostartfiles -nodefaultlibs -nostdlib

#-Wl,-export-dynamic

EXTRA_KERNEL_DEPENDENCIES_SHARED = \
	$(top_srcdir)/kernel/arch/x86_64/kernel.ld \
	$(DUMPDIR)

#$(ZLIBSTATICLIB) \
#$(ZLIBPUBLICINCLUDES)

KERNEL_LDADD_SHARED = -lgcc

# ----------------------------------------------------------------------------
# kernel variations

kernel_generic_SOURCES = $(KERNEL_SOURCES_SHARED)

kernel_generic_CXXFLAGS = $(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED)

kernel_generic_CCASFLAGS = $(KERNEL_CCASFLAGS_SHARED)
	$(KERNEL_INCLUDES_SHARED)

kernel_generic_LDFLAGS = $(KERNEL_LDFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED)

kernel_generic_LDADD = $(KERNEL_LDADD_SHARED)

EXTRA_kernel_generic_DEPENDENCIES = $(EXTRA_KERNEL_DEPENDENCIES_SHARED)

kernel_bmi_SOURCES = $(KERNEL_SOURCES_SHARED)

kernel_bmi_CXXFLAGS = $(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) $(KERNEL_BMI_FLAGS)

kernel_bmi_CFLAGS = $(KERNEL_CXXFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) $(KERNEL_BMI_FLAGS)

kernel_bmi_CCASFLAGS = $(KERNEL_CCASFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) $(KERNEL_BMI_FLAGS)

kernel_bmi_LDFLAGS = $(KERNEL_LDFLAGS_SHARED) \
	$(KERNEL_INCLUDES_SHARED) $(KERNEL_BMI_FLAGS)

kernel_bmi_LDADD = $(KERNEL_LDADD_SHARED)

EXTRA_kernel_bmi_DEPENDENCIES = $(EXTRA_KERNEL_DEPENDENCIES_SHARED)

# ----------------------------------------------------------------------------
# libc

libc_adir = $(includedir)/libc

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

libc_a_SOURCES = \
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
	libc/src/stdlib/_Exit.cc \
	libc/src/fcntl/creat.cc \
	libc/src/fcntl/fcntl.cc \
	libc/src/fcntl/open.cc \
	libc/src/fcntl/openat.cc \
	libc/src/fcntl/posix_fadvise.cc \
	libc/src/fcntl/posix_fallocate.cc \
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
	libc/src/sys/syscall.S \
	$(libc_a_HEADERS)

libc_a_CXXFLAGS = \
	-isystem $(top_srcdir)/libc/include \
	-isystem $(top_builddir)/cross-tool/x86_64-elf/include \
	$(FREESTANDING_FLAGS) \
	-std=c++14 -static \
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
	$(NO_EXCEPTIONS_FLAGS) \
	$(FREESTANDING_FLAGS) \
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

user_shell_SOURCES = \
	user/test/test.S

user_shell_LDFLAGS = \
	-Wl,-z,max-page-size=4096

user_shell_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS)

user_shell_LDADD = libc.a

EXTRA_user_shell_DEPENDENCIES = libc.a

#user_hello_SOURCES =

# ----------------------------------------------------------------------------
# Misc make rules

$(DUMPDIR):
	mkdir -p $(top_builddir)/dump

#include zlib.mk
include symbols.mk
include diskfat.mk
include diskiso.mk

clean-local:
	rm -f $(top_builddir)/mbr.sym
	rm -f $(top_builddir)/mbr.map
	rm -f $(top_builddir)/mbr.dis
	rm -f $(top_builddir)/bootfat.sym
	rm -f $(top_builddir)/bootfat.map
	rm -f $(top_builddir)/bootfat.dis
	rm -f $(top_builddir)/bootiso.sym
	rm -f $(top_builddir)/bootiso.map
	rm -f $(top_builddir)/kernel-generic.sym
	rm -f $(top_builddir)/kernel-bmi.sym
	rm -f $(top_builddir)/kernel-generic.dis
	rm -f $(top_builddir)/kernel-bmi.dis
	rm -f $(top_builddir)/kernel-generic.map
	rm -f $(top_builddir)/kernel-bmi.map

distclean-local:
	rm -rf $(top_builddir)/extern
	rm -f $(top_builddir)/fatdisk.img
	rm -f $(top_builddir)/fatpart.img
	rm -f $(top_builddir)/qemu-emu.mk
	rm -rf $(DUMPDIR)

# ----------------------------------------------------------------------------
# Generate register definitions

generated_headers: \
	$(top_srcdir)/kernel/device/nvmebits.h \
	$(top_srcdir)/kernel/device/pcibits.h \
	$(top_srcdir)/kernel/device/i350bits.h \
	$(top_srcdir)/kernel/device/usb_xhcibits.h \
	$(top_srcdir)/kernel/arch/x86_64/cpu/apicbits.h \
	$(top_srcdir)/kernel/arch/x86_64/cpu/picbits.h

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

$(top_srcdir)/kernel/device/i350bits.h: \
		$(top_srcdir)/kernel/device/i350.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

$(top_srcdir)/kernel/device/usb_xhcibits.h: \
		$(top_srcdir)/kernel/device/usb_xhci.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

$(top_srcdir)/kernel/arch/x86_64/cpu/apicbits.h: \
		$(top_srcdir)/kernel/arch/x86_64/cpu/apic.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

$(top_srcdir)/kernel/arch/x86_64/cpu/picbits.h: \
		$(top_srcdir)/kernel/arch/x86_64/cpu/pic.bits \
		$(top_srcdir)/genregs.py
	mkdir -p "$(@D)"
	rm -f "$@"
	$(top_srcdir)/genregs.py "$<" > "$@"

# ----------------------------------------------------------------------------
# QEMU launcher

$(DUMPDIR)/qemu-debug-out:
	mkdir -p "$(@D)"
	mkfifo "$@"

QEMU = qemu-system-x86_64

QEMU_CPUS = 8
QEMU_CORES = 4
QEMU_THREADS = 1
QEMU_CPU = max,migratable=false,host-cache-info=true

QEMU_RAM = 5G

QEMU_USB = -device nec-usb-xhci,id=xhci \
	-drive if=none,id=stick,file=usb_disk.img,format=raw \
	-device usb-kbd,bus=xhci.0 \
	-device usb-mouse,bus=xhci.0 \
	-device usb-mtp,bus=xhci.0,x-root=$(builddir) \
	-device usb-audio,bus=xhci.0 \
	-device usb-net,bus=xhci.0 \
	-device usb-storage,bus=xhci.0,drive=stick \
	-trace 'usb*'


QEMU_NET = \
	-net nic,model=rtl8139 \
	-net nic,model=ne2k_pci \
	-net nic,model=e1000 \
	$(QEMU_BRIDGE) \
	-net dump,file="$(DUMPDIR)/netdump"

QEMU_MONITOR = \
	-chardev socket,id=qemu-monitor,host=localhost,port=7777,server,nowait,telnet

QEMU_DEBUGCON = \
	-chardev pipe,path=$(DUMPDIR)/qemu-debug-out,id=qemu-debug-out \
	-mon qemu-monitor,mode=readline \
	-device isa-debugcon,chardev=qemu-debug-out

#-chardev socket,id=qemu-serial-socket,host=localhost,port=7778,server,nowait

QEMU_SERIAL = \
	-chardev tty,id=qemu-serial-socket,path=/dev/ttyS0 \
	-serial chardev:qemu-serial-socket

QEMU_COMMON = \
	$(QEMU_MONITOR) \
	$(QEMU_SERIAL) \
	$(QEMU_DEBUGCON) \
	$(QEMU_NET) \
	-no-shutdown -no-reboot -d unimp,guest_errors,cpu_reset \
	-m $(QEMU_RAM) \
	$(QEMU_FLAGS) \
	$(QEMU_USB)

QEMU_RT_debug = -s -S
QEMU_RT_run = -s
QEMU_HDCTL_DEV_nvme = \
	-machine q35 \
	-device nvme,drive=nvmedrv,serial=42424242,bootindex=0
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
QEMU_EMU_tcg = -cpu max
#-cpu EPYC

#QEMU_BIOS = -bios $(top_srcdir)/qemu-q35-20170927.rom
QEMU_BIOS = -bios $(top_srcdir)/qemu-seabios.rom

# Connect to debug log
monitor-debug-output:
	while true; do cat "$(DUMPDIR)/qemu-debug-out"; done

# Connect to console
monitor-connect:
	telnet localhost 7777

# Generate emulators makefile
$(top_builddir)/qemu-emu.mk: $(top_srcdir)/Makefile.am \
		$(top_srcdir)/qemu-emu-generator
	$(top_srcdir)/qemu-emu-generator

-include qemu-emu.mk

# ----------------------------------------------------------------------------
# Bochs launcher

BOCHS=bochs

$(abs_top_builddir)/bochs-debugger-commands: \
		$(top_srcdir)/bochs-debugger-commands
	cp "$<" "$@"

$(abs_top_builddir)/bochs-debugger-boot-commands: \
		$(top_srcdir)/bochs-debugger-boot-commands
	cp "$<" "$@"

debug-iso-bochs: $(QEMU_IMAGE_iso) \
		mbr.sym bootfat.sym \
		kernel-generic.sym kernel-bmi.sym \
		$(top_srcdir)/bochs-iso-config.bxrc \
		$(abs_top_builddir)/bochs-debugger-commands
	$(BOCHS) \
		-f $(top_srcdir)/bochs-iso-config.bxrc \
		-rc bochs-debugger-commands \
		$(BOCHS_FLAGS)

.PHONY: debug-iso-bochs

debug-fat-bochs: $(QEMU_IMAGE_fat) \
		mbr.sym bootfat.sym \
		kernel-generic.sym kernel-bmi.sym \
		$(top_srcdir)/bochs-fat-config.bxrc \
		$(abs_top_builddir)/bochs-debugger-commands
	$(BOCHS) \
		-f $(top_srcdir)/bochs-fat-config.bxrc \
		-rc bochs-debugger-commands \
		$(BOCHS_FLAGS)

.PHONY: debug-fat-bochs

debug-iso-bochs-boot: $(QEMU_IMAGE_iso) \
		mbr.sym bootfat.sym \
		kernel-generic.sym kernel-bmi.sym \
		$(top_srcdir)/bochs-iso-config.bxrc \
		$(abs_top_builddir)/bochs-debugger-boot-commands
	$(BOCHS) \
		-f $(top_srcdir)/bochs-iso-config.bxrc \
		-rc bochs-debugger-boot-commands \
		$(BOCHS_FLAGS)

.PHONY: debug-iso-bochs-boot

debug-fat-bochs-boot: $(QEMU_IMAGE_fat) \
		mbr.sym bootfat.sym \
		kernel-generic.sym kernel-bmi.sym \
		$(top_srcdir)/bochs-fat-config.bxrc \
		$(abs_top_builddir)/bochs-debugger-boot-commands
	$(BOCHS) \
		-f $(top_srcdir)/bochs-fat-config.bxrc \
		-rc bochs-debugger-boot-commands \
		$(BOCHS_FLAGS)

.PHONY: debug-fat-bochs-boot

run-iso-bochs: $(QEMU_IMAGE_iso) \
		mbr.sym bootfat.sym \
		kernel-generic.sym kernel-bmi.sym \
		$(top_srcdir)/bochs-iso-config.bxrc
	$(BOCHS) -f $(top_srcdir)/bochs-iso-config.bxrc -q \
		$(BOCHS_FLAGS)

.PHONY: run-iso-bochs

run-fat-bochs: $(QEMU_IMAGE_fat) \
		mbr.sym bootfat.sym \
		kernel-generic.sym kernel-bmi.sym \
		$(top_srcdir)/bochs-fat-config.bxrc
	$(BOCHS) -f $(top_srcdir)/bochs-fat-config.bxrc -q \
		$(BOCHS_FLAGS)

.PHONY: run-fat-bochs
