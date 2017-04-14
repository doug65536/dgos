# Make bootloader

CSRCS := \
	bootsect.cc \
	screen.cc \
	fs.cc \
	fat32.cc \
	iso9660.cc \
	cpu.cc \
	malloc.cc \
	string.cc \
	utf.cc \
	paging.cc \
	elf64.cc \
	rand.cc \
	physmem.cc \
	farptr.cc \
	vesa.cc \
	progressbar.cc \
	driveinfo.cc \
	debug.cc

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

CXX_IS_CLANG := $(shell ($(CXX) -v 2>&1 | head -n 1 | grep clang > /dev/null) && echo 1)

GDB := gdb
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
TOUCH := touch
RM := rm
SUDO := sudo
CAT := cat
OBJDUMP := objdump
HEXDUMP := hexdump
GENISOIMAGE := genisoimage
ISO_STAGE := cdstage
ISO_FILE := dgos.iso

# Linking libgcc.a crashes my linker
#LIBGCC := $(shell $(CXX) -m32 -print-libgcc-file-name)
#LIBGCCDIR := $(dir $(LIBGCC))
#LIBGCCFILENAME = $(notdir $(LIBGCC))
#LIBGCCNAME := $(patsubst lib%.a,%,$(LIBGCCFILENAME))
#$(info $(LIBGCCNAME))

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.cc=.o)

OBJS := $(AOBJS) $(COBJS)

# PC boot code is guaranteed to be 32-bit architecture
TARGET_ARCH := -m16
TARGET_ARCH_AS := --32

CXXFLAGS := $(TARGET_ARCH) -ggdb -Os -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror \
	-mno-sse -mno-sse2 -mfpmath=387 \
	-fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti \
	-mrtd -mregparm=3 \
	-std=c++14

ifeq ($(CXX),clang++)
	CXX := g++
endif

ifneq ($(CXX_IS_CLANG),1)
	CXXFLAGS += \
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
#WEVERYTHING += -Wbad-function-cast
WEVERYTHING += -Wwrite-strings
#WEVERYTHING += -Wmissing-prototypes
WEVERYTHING += -Wredundant-decls
WEVERYTHING += -fstrict-aliasing

ifneq ($(CXX_IS_CLANG),1)
	#WEVERYTHING += -Wunsuffixed-float-constants
endif

CXXFLAGS += $(WEVERYTHING)

# Optimize
CXXFLAGS += -Os -DNDEBUG

ifdef TUNEFOR
	CXXFLAGS += -mtune=$(TUNEFOR)
endif

LINKERSCRIPT = bootsect.ld

SYMBOLFILE = $(BINDIR)/bootsect-debug.bin
KERNELSYMBOLFILE = $(BINDIR)/../../kernel/bin/kernel
BOOTSECTORS = $(BINDIR)/bootsect.bin
BOCHSSYMBOLS = $(BINDIR)/bochs-symbols
BOCHSKERNELSYMBOLS = $(BINDIR)/bochs-kernel-symbols
BOCHSCOMBINEDSYMBOLS = $(BINDIR)/bochs-combined-symbols

OUTPUTS = $(BOOTSECTORS) $(SYMBOLFILE) $(ISO_FILE) \
	$(DUMPDIR)/bootsect.dis \
	$(DUMPDIR)/bootsect-debug.disasm \
	$(DUMPDIR)/bootsect.hex

NEEDSMALLEST := bootsect.cc
#NOOPTIMIZE := iso9660.cc elf64.cc cpu.cc

DISKIMAGE := $(DISKIMGDIR)/dgos.fat32.raw

LDFLAGS := -m i386linux -no-stdlib -M -T bootsect.ld

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.cc=.o): CXXFLAGS += -Os -DNDEBUG
$(NOOPTIMIZE:.cc=.o): CXXFLAGS += -O0

ISO_DEPENDS := bootsect.bin dgos-kernel hello.km background.png
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
	mkdir -p $(DUMPDIR)
	$(LD) $(LDFLAGS) --oformat elf32-little \
		$(OUTPUT_OPTION) -g $(OBJS) > $(DUMPDIR)/bootsect-debug.map
		#-L$(LIBGCCDIR) -l$(LIBGCCNAME)

$(BOOTSECTORS): $(OBJS) $(LINKERSCRIPT)
	mkdir -p $(DUMPDIR)
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
	touch $(ISO_STAGE)/home/unicode-🂡
	echo Hello! >> $(ISO_STAGE)/root/hello.txt

stagedir: $(ISO_STAGE)/bootsect.bin modules

modules: $(ISO_STAGE)/hello.km

$(ISO_STAGE)/hello.km: ../modules/hello/hello.km
	cp ../modules/hello/hello.km $(ISO_STAGE)

$(ISO_STAGE)/background.png: ../user/background.png
	cp ../user/background.png $(ISO_STAGE)

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
	$(CP) ../user/background.png disk
	$(TOUCH) disk/UCNAME
	$(TOUCH) disk/lcname
	$(TOUCH) disk/UCNAME.UEX
	$(TOUCH) disk/UCNAME.lex
	$(TOUCH) disk/lcname.UEX
	$(TOUCH) disk/lcname.lex
	$(MAKE) umountdisk

.PHONY: all clean
.PHONY: deletedisk bootable-disk debuggable-disk mountdisk umountdisk
.PHONY: kernel debuggable-kernel-disk iso stagedir modules

.PRECIOUS: $(DISKIMAGE)

DISASSEMBLEFLAGS := --adjust-vma=0x7C00 -D -b binary -mi386 -Maddr16,data16
DISASSEMBLEELFFLAGS := -mi386 -Maddr16,data16 --disassemble-all --source

include emulators.mk

include ../kernel/autodep.mk
