# Make bootloader

CSRCS := \
	bootsect.c \
	screen.c \
	fs.c \
	fat32.c \
	iso9660.c \
	cpu.c \
	malloc.c \
	string.c \
	utf.c \
	paging.c \
	elf64.c \
	rand.c \
	physmem.c \
	farptr.c \
	vesa.c \
	progressbar.c \
	driveinfo.c

ASRCS := \
	part.s \
	mpentry.s \
	exception.s

BINDIR := bin
DUMPDIR := dump
DISKIMGDIR ?= diskimg
MOUNTDIR := disk

$(shell mkdir -p $(BINDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)

CC_IS_CLANG := $(shell ($(CC) -v 2>&1 | head -n 1 | grep clang > /dev/null) && echo 1)

GDB := gdb
QEMU := qemu-system-x86_64
SED := sed
BOCHS := bochs
GREP := grep
SORT := sort
DD := dd
MOUNT := mount
UMOUNT := umount
KPARTX := kpartx
MKFS := mkfs.vfat
CP := cp
RM := rm
SUDO := sudo
CAT := cat
OBJDUMP := objdump
HEXDUMP := hexdump
GENISOIMAGE := genisoimage
ISO_STAGE := cdstage
ISO_FILE := dgos.iso

QEMU_MACHINE := q35
QEMU_CPU := kvm64
#Haswell
#kvm64
QEMU_RAM := 5G
QEMU_FLAGS :=  -no-shutdown -no-reboot -d unimp

QEMU_BRIDGE :=

QEMU_NET := \
	-net nic,model=rtl8139 \
	-net nic,model=ne2k_pci \
	-net nic,model=e1000 \
	$(QEMU_BRIDGE) \
	-net dump,file=dump/netdump


# Linking libgcc.a crashes my linker
#LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
#LIBGCCDIR := $(dir $(LIBGCC))
#LIBGCCFILENAME = $(notdir $(LIBGCC))
#LIBGCCNAME := $(patsubst lib%.a,%,$(LIBGCCFILENAME))
#$(info $(LIBGCCNAME))

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.c=.o)

OBJS := $(AOBJS) $(COBJS)

# PC boot code is guaranteed to be 32-bit architecture
TARGET_ARCH := -m16
TARGET_ARCH_AS := --32

CFLAGS := $(TARGET_ARCH) -ggdb -Os -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror -Wmissing-prototypes \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables -fno-exceptions \
	-mrtd -mregparm=3 \
	-std=c11

ifneq ($(CC_IS_CLANG),1)
	CFLAGS += \
		-falign-functions=1 -falign-labels=1 \
		-falign-loops=1 -falign-jumps=1 \
		-mno-align-stringops
endif

#-fomit-frame-pointer
#-mrtd

# Max warnings
#too many false positives WEVERYTHING  = -Wconversion
WEVERYTHING += -Wdouble-promotion
WEVERYTHING += -Wformat=2
WEVERYTHING += -Wfloat-equal
WEVERYTHING += -Wundef
WEVERYTHING += -Wbad-function-cast
WEVERYTHING += -Wwrite-strings
WEVERYTHING += -Wmissing-prototypes
WEVERYTHING += -Wredundant-decls
WEVERYTHING += -fstrict-aliasing

ifneq ($(CC_IS_CLANG),1)
	WEVERYTHING += -Wunsuffixed-float-constants
endif

CFLAGS += $(WEVERYTHING)

# Optimize
#CFLAGS += -Os -DNDEBUG

ASFLAGS := --warn

ifdef TUNEFOR
	CFLAGS += -mtune=$(TUNEFOR)
endif

LINKERSCRIPT = bootsect.ld

SYMBOLFILE = $(BINDIR)/bootsect-debug.bin
KERNELSYMBOLFILE = $(BINDIR)/../../kernel/bin/kernel
BOOTSECTORS = $(BINDIR)/bootsect.bin
BOCHSSYMBOLS = $(BINDIR)/bochs-symbols
BOCHSKERNELSYMBOLS = $(BINDIR)/bochs-kernel-symbols
BOCHSCOMBINEDSYMBOLS = $(BINDIR)/bochs-combined-symbols

QEMU_DEBUGCON := \
	-chardev pipe,path=dump/qemu-debug-out,id=qemu-debug-out \
	-device isa-debugcon,chardev=qemu-debug-out

QEMU_SMP := -smp cpus=4,cores=2,threads=1

OUTPUTS = $(BOOTSECTORS) $(SYMBOLFILE) $(ISO_FILE) \
	$(DUMPDIR)/bootsect.dis \
	$(DUMPDIR)/bootsect-debug.disasm \
	$(DUMPDIR)/bootsect.hex

NEEDSMALLEST := bootsect.c
NOOPTIMIZE := iso9660.c elf64.c cpu.c

DISKIMAGE := $(DISKIMGDIR)/dgos.fat32.raw

LDFLAGS := -m i386linux -no-stdlib -M -T bootsect.ld

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.c=.o): CFLAGS += -Os -DNDEBUG
$(NOOPTIMIZE:.c=.o): CFLAGS += -O0

ISO_DEPENDS := bootsect.bin dgos-kernel hello.km
ISO_TARGETS := $(patsubst %,$(ISO_STAGE)/%,$(ISO_DEPENDS))

#
# Standard targets

all: $(OUTPUTS)

clean:
	@$(RM) -f $(BINDIR)/bootsect*.bin
	@$(RM) -f $(OBJS)
	@$(RM) -f $(DUMPDIR)/*.map
	@$(RM) -f $(OUTPUTS)
	@$(RM) -rf $(ISO_STAGE)
	(cd ../kernel && $(MAKE) clean)

cleaner: clean deletedisk

#
# Link binaries

$(SYMBOLFILE): $(OBJS) $(LINKERSCRIPT)
	$(LD) $(LDFLAGS) --oformat elf32-little \
		$(OUTPUT_OPTION) -g $(OBJS) > $(DUMPDIR)/bootsect-debug.map
		#-L$(LIBGCCDIR) -l$(LIBGCCNAME)

$(BOOTSECTORS): $(OBJS) $(LINKERSCRIPT)
	$(LD) $(LDFLAGS) --oformat binary \
		$(OUTPUT_OPTION) $(OBJS) > $(DUMPDIR)/bootsect.map
		#-L$(LIBGCCDIR) -l$(LIBGCCNAME)

#
# Build kernel
kernel:
	(cd ../kernel && $(MAKE))

$(KERNELSYMBOLFILE): kernel

#
# Create/delete disk image

deletedisk:
	@$(RM) -f $(DISKIMAGE)

# Create disk image
$(DISKIMAGE):
	# Make an 8GB FAT32 partition in a file
	$(MKFS) -C -D 0x80 -F 32 -s 8 -h 128 $(DISKIMAGE) 16384
	#
	# Convert partition into device
	# By copying first 64KB forward by 64KB,
	# then adding 64KB to the end of the file,
	# then clearing the first 64KB,
	# then writing the MBR at the beginning of the disk
	#
	# Copy forward 64KB
	$(DD) if=$(DISKIMAGE) of=$(DISKIMAGE) bs=512 count=128 seek=128
	# Clear first 64KB
	$(DD) if=/dev/zero of=$(DISKIMAGE) bs=512 count=128 conv=notrunc
	# Add 64KB to the end of the file
	$(DD) if=/dev/zero of=$(DISKIMAGE) bs=512 count=0 seek=32896

bootable-disk: $(DISKIMAGE) $(BOOTSECTORS)
	# Inject boot sector (up to 64KB)
	$(DD) if=$(BOOTSECTORS) of=$(DISKIMAGE) bs=512 count=128 conv=notrunc

debuggable-disk: bootable-disk $(SYMBOLFILE)

debuggable-kernel-disk: kernel bootable-disk inject-kernel

#
# ISO

iso: $(ISO_FILE)

$(ISO_STAGE)/bootsect.bin: $(BOOTSECTORS)
	@mkdir -p $(@D)
	$(CP) $< $@
	mkdir -p $(ISO_STAGE)/bin
	mkdir -p $(ISO_STAGE)/boot
	mkdir -p $(ISO_STAGE)/dev
	mkdir -p $(ISO_STAGE)/etc
	mkdir -p $(ISO_STAGE)/home
	mkdir -p $(ISO_STAGE)/lib
	mkdir -p $(ISO_STAGE)/media
	mkdir -p $(ISO_STAGE)/mnt
	mkdir -p $(ISO_STAGE)/opt
	mkdir -p $(ISO_STAGE)/proc
	mkdir -p $(ISO_STAGE)/root
	mkdir -p $(ISO_STAGE)/run
	mkdir -p $(ISO_STAGE)/include
	mkdir -p $(ISO_STAGE)/sbin
	mkdir -p $(ISO_STAGE)/srv
	mkdir -p $(ISO_STAGE)/sys
	mkdir -p $(ISO_STAGE)/tmp
	mkdir -p $(ISO_STAGE)/usr
	mkdir -p $(ISO_STAGE)/usr/bin
	mkdir -p $(ISO_STAGE)/usr/include
	mkdir -p $(ISO_STAGE)/usr/local
	mkdir -p $(ISO_STAGE)/usr/sbin
	mkdir -p $(ISO_STAGE)/usr/share
	mkdir -p $(ISO_STAGE)/usr/src
	mkdir -p $(ISO_STAGE)/var
	mkdir -p $(ISO_STAGE)/var/cache
	mkdir -p $(ISO_STAGE)/var/lib
	mkdir -p $(ISO_STAGE)/var/lock
	mkdir -p $(ISO_STAGE)/var/log
	mkdir -p $(ISO_STAGE)/var/mail
	mkdir -p $(ISO_STAGE)/var/opt
	mkdir -p $(ISO_STAGE)/var/run
	mkdir -p $(ISO_STAGE)/var/spool
	mkdir -p $(ISO_STAGE)/var/tmp
	mkdir -p $(ISO_STAGE)/dev
	echo Hello! >> $(ISO_STAGE)/root/hello.txt

stagedir: $(ISO_STAGE)/bootsect.bin modules

modules: $(ISO_STAGE)/hello.km

$(ISO_STAGE)/hello.km: ../modules/hello/hello.km
	cp ../modules/hello/hello.km $(ISO_STAGE)

$(ISO_STAGE)/dgos-kernel: $(KERNELSYMBOLFILE)
	@mkdir -p $(@D)
	$(CP) $(KERNELSYMBOLFILE) $@

$(ISO_FILE): $(ISO_TARGETS)
	# Do some gymnastics to get the number of 512-byte
	# blocks for -boot-load-size
	( \
		export size=$$(wc -c < "$(ISO_STAGE)/bootsect.bin") && \
		export blocks=$$(( (size + 511) / 512 )) && \
		\
		$(GENISOIMAGE) -input-charset utf8 \
			-o $(ISO_FILE) \
			-b bootsect.bin \
			-iso-level 2 \
			-no-emul-boot \
			-boot-load-size $$blocks \
			-boot-info-table \
			-r -J \
			$(ISO_STAGE) \
	)

run-iso-singlecpu: $(ISO_FILE)
	$(QEMU) $(QEMU_FLAGS) -m $(QEMU_RAM) -s -cdrom $(ISO_FILE) \
		$(QEMU_DEBUGCON) \
		$(QEMU_NET)

run-iso: $(ISO_FILE)
	$(QEMU) -m $(QEMU_RAM) -s -cdrom $(ISO_FILE) \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		$(QEMU_DEBUGCON) \
		$(QEMU_SMP) \
		$(QEMU_NET)

run-iso-singlecpu-kvm: $(ISO_FILE)
	$(QEMU) $(QEMU_FLAGS) -m $(QEMU_RAM) -s -cdrom $(ISO_FILE) \
		$(QEMU_DEBUGCON) \
		$(QEMU_NET) \
		-enable-kvm

run-iso-kvm: $(ISO_FILE)
	$(QEMU) $(QEMU_FLAGS) -m $(QEMU_RAM) -s -cdrom $(ISO_FILE) \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		$(QEMU_DEBUGCON) \
		$(QEMU_SMP) \
		$(QEMU_NET) \
		-enable-kvm

run-iso-numa-kvm: $(ISO_FILE)
	$(QEMU) $(QEMU_FLAGS) -m $(QEMU_RAM) -s -cdrom $(ISO_FILE) \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		$(QEMU_DEBUGCON) \
		$(QEMU_SMP) \
		-numa node,mem=256M,cpus=0-1 \
		-numa node,mem=256M,cpus=2-3 \
		$(QEMU_NET) \
		-enable-kvm

run-iso-numa: $(ISO_FILE)
	$(QEMU) $(QEMU_FLAGS) -m $(QEMU_RAM) -s -cdrom $(ISO_FILE) \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		$(QEMU_DEBUGCON) \
		$(QEMU_SMP) \
		-numa node,mem=256M,cpus=0-1 \
		-numa node,mem=256M,cpus=2-3 \
		$(QEMU_NET)

debug-iso-boot: $(ISO_FILE)
	 $(GDB) --symbols $(SYMBOLFILE) --tui \
		-ex 'b elf64_run' \
		-ex 'target remote | exec $(QEMU) $(QEMU_FLAGS) \
			-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
			-m $(QEMU_RAM) -S -cdrom $(ISO_FILE) \
			-gdb stdio \
			$(QEMU_NET)'

debug-iso-kernel: $(ISO_FILE)
	$(GDB) --symbols $(KERNELSYMBOLFILE) --tui \
		-ex 'b entry' \
		-ex 'target remote | exec $(QEMU) $(QEMU_FLAGS) \
			-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
			-m $(QEMU_RAM) -S -cdrom $(ISO_FILE) \
			-gdb stdio \
			$(QEMU_NET)'

debug-iso-wait-singlecpu: $(ISO_FILE)
	 $(QEMU) $(QEMU_FLAGS) \
		 -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		 -m $(QEMU_RAM) -S -cdrom $(ISO_FILE) -s -S \
		 $(QEMU_DEBUGCON) \
		 $(QEMU_NET)

debug-iso-wait-singlecpu-kvm: $(ISO_FILE)
	 $(QEMU) $(QEMU_FLAGS) \
		 -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		 -m $(QEMU_RAM) -S -cdrom $(ISO_FILE) -s -S \
		 $(QEMU_DEBUGCON) \
		 $(QEMU_NET) \
		 -enable-kvm

bin/mem-garbage:
	dd if=/dev/urandom of=bin/mem-garbage bs=1048576 count=512

debug-iso-wait: $(ISO_FILE) #bin/mem-garbage
	 $(QEMU) $(QEMU_FLAGS) \
		 -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		 $(QEMU_SMP) \
		 -m $(QEMU_RAM) -S -cdrom $(ISO_FILE) -s -S \
		 $(QEMU_DEBUGCON) \
		 $(QEMU_NET)

#		 -object memory-backend-file,id=mem,size=512M,mem-path=bin/mem-garbage,share=on \
#		 -numa node,memdev=mem

debug-iso-wait-many-ahci: $(ISO_FILE)
	 $(QEMU) $(QEMU_FLAGS) \
		 -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		 $(QEMU_SMP) \
		 -m $(QEMU_RAM) -S -cdrom $(ISO_FILE) -s -S \
		 $(QEMU_DEBUGCON) \
		 -device ich9-ahci,id=ahci2 \
		 -device ich9-ahci,id=ahci3 \
		 -device ich9-ahci,id=ahci4

debug-iso-bridge-test-wait: $(ISO_FILE)
	 $(QEMU) $(QEMU_FLAGS) \
		 -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		 $(QEMU_SMP) \
		 -m $(QEMU_RAM) -S -cdrom $(ISO_FILE) -s -S \
		 $(QEMU_DEBUGCON) \
		 -device i82801b11-bridge,bus=pcie.0,id=pcie.1 \
		 -device i82801b11-bridge,bus=pcie.1,id=pcie.2 \
		 -device i82801b11-bridge,bus=pcie.2,id=pcie.3 \
		 -device i82801b11-bridge,bus=pcie.3,id=pcie.4 \
		 -device i82801b11-bridge,bus=pcie.4,id=pcie.5 \
		 -device i82801b11-bridge,bus=pcie.5,id=pcie.6 \
		 -device ich9-ahci,bus=pcie.1,id=ahci2 \
		 -device ich9-ahci,bus=pcie.2,id=ahci3 \
		 -device ich9-ahci,bus=pcie.2,id=ahci4 \
		 -device ich9-ahci,bus=pcie.4,id=ahci5 \
		 -device ich9-ahci,bus=pcie.5,id=ahci6 \
		 -device ich9-ahci,bus=pcie.6,id=ahci7 \
		 -device ich9-ahci,bus=pcie.6,id=ahci8

debug-iso-wait-kvm: $(ISO_FILE)
	 $(QEMU) $(QEMU_FLAGS) \
		 -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		 $(QEMU_SMP) \
		 $(QEMU_DEBUGCON) \
		 -m $(QEMU_RAM) -S -cdrom $(ISO_FILE) -s -S \
		 -enable-kvm

#
# Debug in KVM

$(BINDIR)/debug-kvm-gdbcommands: utils/debug-kvm-commands.template
	$(SED) "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

$(BINDIR)/debug-kernel-kvm-gdbcommands: utils/debug-kernel-kvm-commands.template
	$(SED) "s|\$$SYMBOLFILE|$(KERNELSYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

debug: all debuggable-disk $(BINDIR)/debug-kvm-gdbcommands
	$(GDB) -x $(BINDIR)/debug-kvm-gdbcommands

debug-kernel: all $(BINDIR)/debug-kernel-kvm-gdbcommands
	$(GDB) -x $(BINDIR)/debug-kernel-kvm-gdbcommands

debug-attach:
	$(GDB) --symbols=$(KERNELSYMBOLFILE) --tui \
		-iex 'set architecture i386:x86-64' \
		-ex 'target remote localhost:1234' \
		-ex 'layout regs' \
		-ex 'set radix 16' \
		-ex 'b assert_failed' \
		-ex 'b unhandled_exception_handler'

debug-boot-attach:
	$(GDB) --symbols $(SYMBOLFILE) --tui \
		-iex 'set architecture i8086' \
		-ex 'target remote localhost:1234' \
		-ex 'layout regs' \
		-ex 'set radix 16' \
		-ex 'b *0x7C00'

run-debug: all bootable-disk debuggable-kernel-disk inject-kernel
	$(QEMU) \
		-drive file=$(DISKIMAGE),format=raw \
		-no-shutdown -no-reboot -d cpu_reset \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		-s -m $(QEMU_RAM) \
		$(QEMU_SMP) \
		$(QEMU_DEBUGCON) \
		$(QEMU_NET)

# -drive file=$(DISKIMAGE),format=raw,if=ide

run-debug-singlecpu: all debuggable-kernel-disk
	$(QEMU) -s \
		-drive file=$(DISKIMAGE),format=raw \
		-no-shutdown -no-reboot -d cpu_reset \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		-m $(QEMU_RAM) \
		$(QEMU_DEBUGCON) \
		$(QEMU_NET)

#		-drive id=disk,file=$(DISKIMAGE),if=none \
#		-device ahci,id=ahci \
#		-device ide-drive,drive=disk,bus=ahci.0 \

run-debug-wait: all debuggable-kernel-disk
	$(QEMU) -s -S  \
		-drive file=$(DISKIMAGE),format=raw \
		-no-shutdown -no-reboot -d cpu_reset,guest_errors \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		-m $(QEMU_RAM) \
		$(QEMU_SMP) \
		$(QEMU_DEBUGCON) \
		$(QEMU_NET)

run-debug-wait-singlecpu: all debuggable-kernel-disk
	$(QEMU) -s -S  \
		-drive file=$(DISKIMAGE),format=raw \
		-no-shutdown -no-reboot -d cpu_reset,guest_errors \
		-machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
		-m $(QEMU_RAM) \
		$(QEMU_DEBUGCON) \
		$(QEMU_NET)

monitor-debug-output:
	while true; do cat dump/qemu-debug-out; done

run: all bootable-disk
	$(QEMU) -m 5G -drive file=$(DISKIMAGE),format=raw \
		$(QEMU_NET)

#		-netdev user,id=mynet0,net=192.168.66.0/24,dhcpstart=192.168.66.9 \
#		-net nic,model=i82551 \
#		-net nic,model=i82557b \
#		-net nic,model=i82559er \
#		-net nic,model=pcnet \
#		-net nic,model=smc91c111 \
#		-net nic,model=lance \
#		-net nic,model=ne2k_isa \
#		-net nic,model=mcf_fec

#
# Debug in bochs

bochs-symbols: $(BOCHSCOMBINEDSYMBOLS)

$(BOCHSCOMBINEDSYMBOLS): $(BOCHSKERNELSYMBOLS) $(BOCHSSYMBOLS)
	$(CAT) $^ > $@

$(BOCHSKERNELSYMBOLS): kernel
	$(OBJDUMP) --wide --syms ../kernel/bin/kernel | \
		$(GREP) -P '^[0-9A-Fa-f]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

$(BOCHSSYMBOLS): $(SYMBOLFILE)
	$(OBJDUMP) --wide --syms $^ | \
		$(GREP) -P '^[0-9A-Fa-f]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

$(BINDIR)/bochs-config.bxrc: utils/bochs-config.bxrc.template
	$(SED) "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

debug-bochs: all bochs-symbols iso $(BINDIR)/bochs-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-config.bxrc \
		-rc utils/bochs-debugger-commands

debug-bochs-boot: all bochs-symbols iso $(BINDIR)/bochs-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-config.bxrc \
		-rc utils/bochs-debugger-boot-commands

run-bochs: all bochs-symbols debuggable-disk $(BINDIR)/bochs-config.bxrc
	$(BOCHS) -qf $(BINDIR)/bochs-config.bxrc -q

#
# Mount/unmount disk image through loop device

mountdisk:
	@mkdir -p $(MOUNTDIR) >/dev/null
	SAVEUID=$$(id -u) && \
	SAVEGID=$$(id -g) && \
	LOOPNAME=$$($(SUDO) $(KPARTX) -avs $(DISKIMAGE) | \
		$(GREP) -oP '(?<=add map )(loop\S+)') && \
	$(SUDO) $(MOUNT) /dev/mapper/$$LOOPNAME $(MOUNTDIR) \
		-o rw,uid=$$SAVEUID,gid=$$SAVEGID

umountdisk:
	$(SUDO) $(UMOUNT) $(MOUNTDIR)
	$(SUDO) $(KPARTX) -d $(DISKIMAGE)

inject-kernel: all
	$(MAKE) mountdisk
	(cd ../kernel && $(MAKE))
	$(CP) ../kernel/bin/kernel disk/dgos-kernel
	$(MAKE) umountdisk

.PHONY: all clean run debug-bochs debug bochs-symbols
.PHONY: deletedisk bootable-disk debuggable-disk mountdisk umountdisk
.PHONY: kernel debuggable-kernel-disk
.PHONY: iso debug-iso-boot debug-iso-kernel
.PHONY: stagedir modules

.PRECIOUS: $(DISKIMAGE)

DISASSEMBLEFLAGS := --adjust-vma=0x7C00 -D -b binary -mi386 -Maddr16,data16
DISASSEMBLEELFFLAGS := -mi386 -Maddr16,data16 --disassemble --source

include ../kernel/autodep.mk
