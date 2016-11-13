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
	farptr.c

ASRCS := \
	part.s \
	exception.s

BINDIR := bin
DUMPDIR := dump
DISKIMGDIR ?= diskimg
MOUNTDIR := disk

$(shell mkdir -p $(BINDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)

GDB ?= gdb-multiarch

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
TARGET_ARCH := -m32
TARGET_ARCH_AS := --32

CFLAGS ?= -ggdb -Os -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror -Wmissing-prototypes \
	-falign-functions=1 -falign-labels=1 \
	-falign-loops=1 -falign-jumps=1 -mno-sse -mno-sse2 -mfpmath=387

#	-mmmx -msse -msse2 -mfpmath=sse

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

ASFLAGS ?= --warn

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
	$(DUMPDIR)/bootsect.dis $(DUMPDIR)/bootsect.hex

NEEDSMALLEST := bootsect.c


DISKIMAGE ?= $(DISKIMGDIR)/dgos.fat32.raw

LDFLAGS ?= -m i386linux -no-stdlib -M -T bootsect.ld

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.c=.o): CFLAGS += -Os -DNDEBUG

#
# Standard targets

all: $(OUTPUTS)

clean:
	@rm -f $(BINDIR)/bootsect*.bin
	@rm -f $(OBJS)
	@rm -f $(DUMPDIR)/*.map
	@rm -f $(OUTPUTS)
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
	@rm -f $(DISKIMAGE)

# Create disk image
$(DISKIMAGE):
	# Make an 8GB FAT32 partition in a file
	mkfs.vfat -C -D 0x80 -F 32 -s 8 -h 128 $(DISKIMAGE) 8388608
	#
	# Convert partition into device
	# By copying first 64KB forward by 64KB,
	# then adding 64KB to the end of the file,
	# then clearing the first 64KB,
	# then writing the MBR at the beginning of the disk
	#
	# Copy forward 64KB
	dd if=$(DISKIMAGE) of=$(DISKIMAGE) bs=512 count=128 seek=128
	# Clear first 64KB
	dd if=/dev/zero of=$(DISKIMAGE) bs=512 count=128 conv=notrunc
	# Add 64KB to the end of the file (16777216+128) + 100 hack
	dd if=/dev/zero of=$(DISKIMAGE) bs=512 count=0 seek=16777414

bootable-disk: $(DISKIMAGE) $(BOOTSECTORS)
	# Inject boot sector (up to 64KB)
	dd if=$(BOOTSECTORS) of=$(DISKIMAGE) bs=1 count=65536 conv=notrunc

debuggable-disk: bootable-disk $(SYMBOLFILE)

debuggable-kernel-disk: kernel bootable-disk inject-kernel

#
# Debug in KVM

$(BINDIR)/debug-kvm-gdbcommands: utils/debug-kvm-commands.template
	sed "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		sed "s|\$$DISKIMAGE|$(DISKIMAGE)|g" | \
		sort > $@

$(BINDIR)/debug-kernel-kvm-gdbcommands: utils/debug-kernel-kvm-commands.template
	sed "s|\$$SYMBOLFILE|$(KERNELSYMBOLFILE)|g" $< | \
		sed "s|\$$DISKIMAGE|$(DISKIMAGE)|g" | \
		sort > $@

debug: all debuggable-disk $(BINDIR)/debug-kvm-gdbcommands
	$(GDB) -x $(BINDIR)/debug-kvm-gdbcommands

debug-kernel: all $(BINDIR)/debug-kernel-kvm-gdbcommands
	$(GDB) -x $(BINDIR)/debug-kernel-kvm-gdbcommands

run-debug: all bootable-disk debuggable-kernel-disk inject-kernel
	qemu-system-x86_64 -drive file=$(DISKIMAGE),format=raw -no-shutdown -no-reboot -d cpu_reset -s

#,guest_errors,unimp

run: all bootable-disk
	qemu-system-x86_64 -m 8G -drive file=$(DISKIMAGE),format=raw \
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
	cat $^ > $@

$(BOCHSKERNELSYMBOLS): kernel
	objdump --wide --syms ../kernel/bin/kernel | \
		grep -P '^[0-9A-Fa-f]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		sed -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		sort > $@

$(BOCHSSYMBOLS): $(SYMBOLFILE)
	objdump --wide --syms $^ | \
		grep -P '^[0-9A-Fa-f]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		sed -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		sort > $@

$(BINDIR)/bochs-config.bxrc: utils/bochs-config.bxrc.template
	sed "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		sed "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

debug-bochs: all bochs-symbols debuggable-kernel-disk $(BINDIR)/bochs-config.bxrc
	bochs -qf $(BINDIR)/bochs-config.bxrc -rc utils/bochs-debugger-commands

debug-bochs-boot: all bochs-symbols debuggable-disk $(BINDIR)/bochs-config.bxrc
	bochs -qf $(BINDIR)/bochs-config.bxrc -rc utils/bochs-debugger-boot-commands

run-bochs: all bochs-symbols debuggable-disk $(BINDIR)/bochs-config.bxrc
	bochs -qf $(BINDIR)/bochs-config.bxrc -q

#
# Mount/unmount disk image through loop device

mountdisk:
	@mkdir -p $(MOUNTDIR) >/dev/null
	SAVEUID=$$(id -u) && \
	SAVEGID=$$(id -g) && \
	LOOPNAME=$$(sudo kpartx -avs $(DISKIMAGE) | grep -oP '(?<=add map )(loop\S+)') && \
	sudo mount /dev/mapper/$$LOOPNAME $(MOUNTDIR) -o rw,uid=$$SAVEUID,gid=$$SAVEGID

umountdisk:
	sudo umount $(MOUNTDIR)
	sudo kpartx -d $(DISKIMAGE)

inject-kernel: all
	$(MAKE) mountdisk
	(cd ../kernel && $(MAKE))
	cp ../kernel/bin/kernel disk/dgos-kernel
	$(MAKE) umountdisk

.PHONY: all clean run debug-bochs debug bochs-symbols
.PHONY: deletedisk bootable-disk debuggable-disk mountdisk umountdisk
.PHONY: kernel debuggable-kernel-disk

.PRECIOUS: $(DISKIMAGE)

DISASSEMBLEFLAGS := --adjust-vma=0x7C00 -D -b binary -mi386 -Maddr16,data16

include ../kernel/autodep.mk
