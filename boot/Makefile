# Make bootloader

CSRCS := \
	bootsect.c \
	screen.c \
	fat32.c \
	cpu.c \
	malloc.c \
	string.c \
	utf.c \
	paging.c \
	elf64.c \
	rand.c \
	physmem.c \
	farptr.c \
	vesa.c

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

# Linking libgcc.a crashes my linker
#LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
#LIBGCCDIR := $(dir $(LIBGCC))
#LIBGCCFILENAME = $(notdir $(LIBGCC))
#LIBGCCNAME := $(patsubst lib%.a,%,$(LIBGCCFILENAME))

$(info $(LIBGCCNAME))

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.c=.o)

OBJS := $(AOBJS) $(COBJS)

# PC boot code is guaranteed to be 32-bit architecture
TARGET_ARCH := -m16
TARGET_ARCH_AS := --32

CFLAGS := $(TARGET_ARCH) -ggdb -Os -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror -Wmissing-prototypes \
	-falign-functions=1 -falign-labels=1 \
	-falign-loops=1 -falign-jumps=1 \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables -fno-exceptions \
	-mrtd \
	-fno-schedule-insns -fno-schedule-insns2 \
	-mregparm=3 -mno-align-stringops

#-fomit-frame-pointer
#-mrtd

#
#

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
WEVERYTHING += -Wunsuffixed-float-constants
WEVERYTHING += -fstrict-aliasing

CFLAGS += $(WEVERYTHING)

# Optimize
CFLAGS += -O0 -DNDEBUG

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

OUTPUTS = $(BOOTSECTORS) $(SYMBOLFILE) \
	$(DUMPDIR)/bootsect.dis \
	$(DUMPDIR)/bootsect-debug.disasm \
	$(DUMPDIR)/bootsect.hex

NEEDSMALLEST := bootsect.c


DISKIMAGE := $(DISKIMGDIR)/dgos.fat32.raw

LDFLAGS := -m i386linux -no-stdlib -M -T bootsect.ld

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.c=.o): CFLAGS += -Os -DNDEBUG

#
# Standard targets

all: $(OUTPUTS)

clean:
	@$(RM) -f $(BINDIR)/bootsect*.bin
	@$(RM) -f $(OBJS)
	@$(RM) -f $(DUMPDIR)/*.map
	@$(RM) -f $(OUTPUTS)
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

#
# Create/delete disk image

deletedisk:
	@$(RM) -f $(DISKIMAGE)

# Create disk image
$(DISKIMAGE):
	# Make an 8GB FAT32 partition in a file
	$(MKFS) -C -D 0x80 -F 32 -s 8 -h 128 $(DISKIMAGE) 8388608
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
	# Add 64KB to the end of the file (16777216+128) + 100 hack
	$(DD) if=/dev/zero of=$(DISKIMAGE) bs=512 count=0 seek=16777414

bootable-disk: $(DISKIMAGE) $(BOOTSECTORS)
	# Inject boot sector (up to 64KB)
	$(DD) if=$(BOOTSECTORS) of=$(DISKIMAGE) bs=1 count=65536 conv=notrunc

debuggable-disk: bootable-disk $(SYMBOLFILE)

debuggable-kernel-disk: kernel bootable-disk inject-kernel

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
		-ex 'set $$rbp=1' \
		-ex 'b mmu_init'

run-debug: all bootable-disk debuggable-kernel-disk inject-kernel
	$(QEMU) -drive file=$(DISKIMAGE),format=raw \
		-no-shutdown -no-reboot -d cpu_reset \
		-s -m 6G -machine q35 -smp cpus=2,cores=2

#,guest_errors,unimp

run: all bootable-disk
	$(QEMU) -m 5G -drive file=$(DISKIMAGE),format=raw \
		-net user \
		-net nic,model=rtl8139 \
		-net nic,model=ne2k_pci \
		-net nic,model=e1000

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

debug-bochs: all bochs-symbols debuggable-kernel-disk $(BINDIR)/bochs-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-config.bxrc \
		-rc utils/bochs-debugger-commands

debug-bochs-boot: all bochs-symbols debuggable-disk $(BINDIR)/bochs-config.bxrc
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

.PRECIOUS: $(DISKIMAGE)

DISASSEMBLEFLAGS := --adjust-vma=0x7C00 -D -b binary -mi386 -Maddr16,data16
DISASSEMBLEELFFLAGS := -mi386 -Maddr16,data16 --disassemble --source

include ../kernel/autodep.mk
