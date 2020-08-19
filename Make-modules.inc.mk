
#==========
# Common module declarations

KERNEL_MODULE_CXXFLAGS_SHARED = \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
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
	-I$(top_srcdir)/user/libutf \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(NO_COMMON_FLAGS) \
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
	libkm.a

KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED = \
	$(top_srcdir)/kernel/arch/x86_64/module.ld \
	libkm.a

KERNEL_MODULE_LDFLAGS_FN = \
	$(KERNEL_MODULE_LDFLAGS_SHARED) \
	-Wl,-Map,$(1).km.map \
	-Wl,-z,now \
	libkm.a

#==========
# Shared by all modules, provides base startup code, module_main call, etc.

noinst_LIBRARIES += libkm.a

libkm_a_SOURCES = \
	modules/libkm/module_entry.cc \
	modules/libkm/module_main.cc \
	modules/libkm/dso_handle.S \
	modules/libkm/__cxa_pure_virtual.S \
	modules/libkm/unwind_resume.S

libkm_a_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED) -static

libkm_a_CCASFLAGS = \
	$(libkm_a_CXXFLAGS)

EXTRA_libkm_a_DEPENDENCIES =

#==========
# perf-like module that does sample based hardwarre
# performance counter analysis from a REPL on a
# terminal through serial

bin_PROGRAMS += symsrv.km
generate_symbols_list += symsrv.km
generate_kallsym_list += symsrv.km

symsrv_km_SOURCES = \
	kernel/device/symbol_server/symbol_server.cc \
	kernel/device/symbol_server/symbol_server.h

symsrv_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

symsrv_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,symsrv)

symsrv_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_symsrv_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# XHCI usb host controller

bin_PROGRAMS += usbxhci.km
generate_symbols_list += usbxhci.km
generate_kallsym_list += usbxhci.km

usbxhci_km_SOURCES = \
	kernel/device/usb_xhci/usb_xhci.cc

usbxhci_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

usbxhci_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,usbxhci)

usbxhci_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_usbxhci_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# USB mass-storage storage controller (flash drives, external enclosures, etc.)

bin_PROGRAMS += usbmsc.km
generate_symbols_list += usbmsc.km
generate_kallsym_list += usbmsc.km

usbmsc_km_SOURCES = \
	kernel/device/usb_storage/usb_storage.cc \
	kernel/device/usb_storage/usb_storage.h

usbmsc_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

usbmsc_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,usbmsc)

usbmsc_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_usbmsc_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# Simple but widely emulated NIC

bin_PROGRAMS += rtl8139.km
generate_symbols_list += rtl8139.km
generate_kallsym_list += rtl8139.km

rtl8139_km_SOURCES = \
	kernel/device/rtl8139/rtl8139.cc

rtl8139_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

rtl8139_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,rtl8193)

rtl8139_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_rtl8139_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# NVMe storage controller driver

bin_PROGRAMS += nvme.km
generate_symbols_list += nvme.km
generate_kallsym_list += nvme.km

nvme_km_SOURCES = \
	kernel/device/nvme/nvme.cc \
	kernel/device/nvme/nvme.h

nvme_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

nvme_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,nvme)

nvme_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_nvme_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# Kernel unit test module

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
	kernel/unittest/test_filesystem.cc \
	kernel/unittest/test_pipe.cc

unittest_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

unittest_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,unittest)

unittest_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_unittest_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# PC keyboard driver

bin_PROGRAMS += keyb8042.km
generate_symbols_list += keyb8042.km
generate_kallsym_list += keyb8042.km

keyb8042_km_SOURCES = \
	kernel/device/keyb8042/keyb8042.cc

keyb8042_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

keyb8042_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,keyb8042)

keyb8042_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_keyb8042_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# AHCI storage controller driver

bin_PROGRAMS += ahci.km
generate_symbols_list += ahci.km
generate_kallsym_list += ahci.km

ahci_km_SOURCES = \
	kernel/device/ahci/ahci.cc

ahci_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

ahci_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,ahci)

ahci_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_ahci_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# IDE storage controller driver

bin_PROGRAMS += ide.km
generate_symbols_list += ide.km
generate_kallsym_list += ide.km


ide_km_SOURCES = \
	kernel/device/ide/ide.cc

ide_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

ide_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,ide)

ide_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_ide_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# Shared base virtio layer

bin_PROGRAMS += virtio-base.km
generate_symbols_list += virtio-base.km
generate_kallsym_list += virtio-base.km

virtio_base_km_SOURCES = \
	kernel/device/virtio-base/virtio-base.cc \
	kernel/device/virtio-base/virtio-base.h

virtio_base_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

virtio_base_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,virtio-base)

virtio_base_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_virtio_base_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# virtio block storage driver

bin_PROGRAMS += virtio-blk.km
generate_symbols_list += virtio-blk.km
generate_kallsym_list += virtio-blk.km


virtio_blk_km_SOURCES = \
	kernel/device/virtio-blk/virtio-blk.cc \
	kernel/device/virtio-blk/virtio-blk.h

virtio_blk_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

virtio_blk_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,virtio-blk)

virtio_blk_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED) virtio-base.km

EXTRA_virtio_blk_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED) \
	virtio-base.km

#==========
# virtio gl/framebuffer display driver

bin_PROGRAMS += virtio-gpu.km
generate_symbols_list += virtio-gpu.km
generate_kallsym_list += virtio-gpu.km

virtio_gpu_km_SOURCES = \
	kernel/device/virtio-gpu/virtio-gpu.cc \
	kernel/device/virtio-gpu/virtio-gpu.h

virtio_gpu_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

virtio_gpu_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,virtio-gpu)

virtio_gpu_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED) virtio-base.km

EXTRA_virtio_gpu_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED) \
	virtio-base.km

#==========
# ext4 filesystem module

bin_PROGRAMS += ext4.km
generate_symbols_list += ext4.km
generate_kallsym_list += ext4.km

ext4_km_SOURCES = \
	kernel/fs/ext4/ext4.cc

ext4_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

ext4_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,ext4)

ext4_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_ext4_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# FAT32 filesystem module

bin_PROGRAMS += fat32.km
generate_symbols_list += fat32.km
generate_kallsym_list += fat32.km

fat32_km_SOURCES = \
	kernel/fs/fat32/fat32.cc \
	kernel/fs/fat32/fat32_decl.h

fat32_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

fat32_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,fat32)

fat32_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_fat32_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# ES137x driver

bin_PROGRAMS += es137x.km
generate_symbols_list += es137x.km
generate_kallsym_list += es137x.km

es137x_km_SOURCES = \
	kernel/device/es137x/es137x.cc \
	kernel/device/es137x/es137x.bits \
	kernel/device/es137x/es137x.bits.h

es137x_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

es137x_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,es137x)

es137x_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_es137x_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)


#==========
# CD/DVD filesystem module

bin_PROGRAMS += iso9660.km
generate_symbols_list += iso9660.km
generate_kallsym_list += iso9660.km

iso9660_km_SOURCES = \
	kernel/fs/iso9660/iso9660.cc \
	kernel/fs/iso9660/iso9660_decl.h \
	kernel/fs/iso9660/iso9660_part.cc

iso9660_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

iso9660_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,iso9660)

iso9660_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_iso9660_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# MBR partition table parser

bin_PROGRAMS += mbr.km
generate_symbols_list += mbr.km
generate_kallsym_list += mbr.km

mbr_km_SOURCES = \
	kernel/fs/mbr/mbr.cc

mbr_km_CXXFLAGS = \
	$(KERNEL_MODULE_CXXFLAGS_SHARED)

mbr_km_LDFLAGS = \
	$(call KERNEL_MODULE_LDFLAGS_FN,mbr)

mbr_km_LDADD = \
	$(KERNEL_MODULE_LDADD_SHARED)

EXTRA_mbr_km_DEPENDENCIES = \
	$(KERNEL_MODULE_EXTRA_DEPENDENCIES_SHARED)

#==========
# GPT partition table parser

bin_PROGRAMS += gpt.km
generate_symbols_list += gpt.km
generate_kallsym_list += gpt.km

gpt_km_SOURCES = \
	kernel/fs/gpt/gpt.cc

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
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS)

user_shell_CFLAGS = \
	-g \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS)

user_shell_SOURCES = \
	user/test/test.S

user_shell_LDFLAGS = \
	-Wl,-z,max-page-size=4096

user_shell_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	-isystem sysroot/include

EXTRA_user_shell_DEPENDENCIES = \
	$(ALL_STDLIB_INSTALLED)

#==========

bin_PROGRAMS += init
generate_symbols_list += init

init_SOURCES = \
	user/init/init.cc \
	user/init/frameserver.cc \
	user/init/frameserver.h

init_CXXFLAGS = \
	-DFROMCXXFLAGS  \
	-isystem sysroot/include \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

init_CFLAGS = \
	-DFROMCFLAGS  \
	-ggdb \
	-isystem sysroot/include \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

init_LDFLAGS = -Lsysroot/lib
#-Wl,-Bdynamic $(USER64_LDFLAGS) $(ELF64_FLAGS) -nostdlib
#-Wl,--no-eh-frame-hdr

init_LDADD = \
	libpng.a libutf.a libz.a u_vga16.o

#sysroot/lib/64/crt0.o
#sysroot/lib/64/libc.a
#sysroot/lib/64/libstdc++.a
#sysroot/lib/64/libm.a

init_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	-isystem sysroot/include

EXTRA_init_DEPENDENCIES = \
	$(top_srcdir)/user/user64_phdrs.ld \
	$(ALL_STDLIB_INSTALLED)

#===========

bin_PROGRAMS += init-shared
generate_symbols_list += init-shared

init_shared_CXXFLAGS = \
	-DFROMCXXFLAGS  \
	-ggdb \
	-isystem sysroot/include \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS) \
	$(TLS_MODEL_FLAGS)

init_shared_CFLAGS = \
	-DFROMCFLAGS  \
	-ggdb \
	-isystem sysroot/include \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	$(USER64_FLAGS) \
	$(USER64_EXE_FLAGS)

init_shared_SOURCES = \
	$(init_SOURCES)

init_shared_LDFLAGS = -static
#-Wl,--no-eh-frame-hdr

#if userspace_so
#init_shared_LDADD = libc.so -lpng -lz
#endif

init_shared_LDADD = -lpng -lz libutf.a u_vga16.o

init_shared_CCASFLAGS = \
	$(ASM_DEBUG_INFO_FLAGS) \
	$(COMPILER_FLAGS) \
	$(OPTIMIZE_SPEED_FLAGS) \
	-isystem sysroot/include

EXTRA_init_shared_DEPENDENCIES = \
	$(top_srcdir)/user/user64_phdrs.ld \
	libutf.a \
	$(ALL_STDLIB_INSTALLED)
