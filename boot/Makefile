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
	rand.c

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

CFLAGS ?= -ggdb -O0 -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror -Wmissing-prototypes \
	-falign-functions=1 -falign-labels=1 \
	-falign-loops=1 -falign-jumps=1 -msse -msse2
	#-mmmx -msse -msse2 -mfpmath=sse

ASFLAGS ?= --warn

ifdef TUNEFOR
	CFLAGS += -mtune=$(TUNEFOR)
endif

LINKERSCRIPT = bootsect.ld

SYMBOLFILE = $(BINDIR)/bootsect-debug.bin
BOOTSECTORS = $(BINDIR)/bootsect.bin
BOCHSSYMBOLS = $(BINDIR)/bochs-symbols

OUTPUTS = $(BOOTSECTORS) $(SYMBOLFILE) \
	$(DUMPDIR)/bootsect.dis $(DUMPDIR)/bootsect.hex

NEEDSMALLEST := bootsect.c
#malloc.c cpu.c screen.c fat32.c

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

debuggable-kernel-disk: kernel bootable-disk
	$(MAKE) inject-kernel

#
# Debug in KVM

$(BINDIR)/debug-kvm-gdbcommands: utils/debug-kvm-commands.template
	sed "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | sed "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

$(BINDIR)/debug-kernel-kvm-gdbcommands: utils/debug-kernel-kvm-commands.template
	sed "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | sed "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

debug: all debuggable-disk $(BINDIR)/debug-kvm-gdbcommands
	gdb -x $(BINDIR)/debug-kvm-gdbcommands

debug-kernel: all debuggable-kernel-disk $(BINDIR)/debug-kernel-kvm-gdbcommands
	gdb -x $(BINDIR)/debug-kernel-kvm-gdbcommands

run-debug: all bootable-disk
	qemu-system-x86_64 -drive file=$(DISKIMAGE),format=raw  -no-shutdown -no-reboot -d int,cpu_reset

#,guest_errors,unimp

run: all bootable-disk
	qemu-system-x86_64 -drive file=$(DISKIMAGE),format=raw

#
# Debug in bochs

$(BOCHSSYMBOLS): $(SYMBOLFILE)
	objdump --syms bin/bootsect-debug.bin | \
		grep -P '^[0-9A-Fa-f]+\s.*\s[a-zA-Z_]\S+$$' | \
		sed -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' > $@

$(BINDIR)/bochs-config.bxrc: utils/bochs-config.bxrc.template
	sed "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | sed "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

debug-bochs: all $(BOCHSSYMBOLS) debuggable-disk $(BINDIR)/bochs-config.bxrc
	bochs -f $(BINDIR)/bochs-config.bxrc -rc utils/bochs-debugger-commands

run-bochs: all debuggable-disk $(BINDIR)/bochs-config.bxrc
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

inject-kernel:
	$(MAKE) mountdisk
	(cd ../kernel && $(MAKE))
	cp ../kernel/bin/kernel disk/dgos-kernel
	$(MAKE) umountdisk

.PHONY: all clean run debug-bochs debug
.PHONY: deletedisk bootable-disk debuggable-disk mountdisk umountdisk
.PHONY: kernel debuggable-kernel-disk

.PRECIOUS: $(DISKIMAGE)

#
# C and C++ autodependencies and compilation
#

# Directory to store auto-generated dependency makefile fragments
DEPDIR := .d

# Make sure dependency file directory exists
$(shell mkdir -p $(DEPDIR) >/dev/null)

# Flags to use when generating autodependencies
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

# Compile commands for C and C++
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE_TO_ASM.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -S
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.s = $(AS) $(ASFLAGS) $(TARGET_ARCH_AS) -c

# Command to move generated dependency files into separate directory
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

OUTPUT_OPTION = -o $@

# Compile assembly
%.o : %.s
%.o : %.s
	$(COMPILE.s) $(OUTPUT_OPTION) $<

# Compile C
%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

# Compile C++ with cc extension
%.o : %.cc
%.o : %.cc $(DEPDIR)/%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

# Compile C++ with cxx extension
%.o : %.cxx
%.o : %.cxx $(DEPDIR)/%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

# Compile C++ with cpp extension
%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

# Compile C to assembly
$(DUMPDIR)/%.s : %.c
$(DUMPDIR)/%.s : %.c
	$(COMPILE_TO_ASM.c) $(OUTPUT_OPTION) $<

# Disassemble
$(DUMPDIR)/%.dis : $(BINDIR)/%.bin
$(DUMPDIR)/%.dis : $(BINDIR)/%.bin
	objdump --adjust-vma=0x7C00 -D -b binary -mi386 -Maddr16,data16 $< > $@

# Hex Dump
$(DUMPDIR)/%.hex : $(BINDIR)/%.bin
$(DUMPDIR)/%.hex : $(BINDIR)/%.bin
	hexdump -C $< > $@

# Tolerate the dependency files being missing
$(DEPDIR)/%.d: ;

# Prevent deletion of generated dependencies
.PRECIOUS: $(DEPDIR)/%.d

# Include the generated makefile fragments
-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(CSRCS)))
