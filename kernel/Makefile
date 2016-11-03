# Make kernel

CSRCS := \
	main.c

ASRCS := entry.s

BINDIR := bin
DUMPDIR := dump

$(shell mkdir -p $(BINDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.c=.o)

OBJS := $(AOBJS) $(COBJS)

TARGET_ARCH := -m64
TARGET_ARCH_AS := --64

CFLAGS ?= -g3 -O0 -ffreestanding \
	-Wall -Wextra -Wpedantic -Werror \
	-Wno-unused-function -fpic -mno-red-zone

#-mcmodel=large

ifdef TUNEFOR
	CFLAGS += -mtune=$(TUNEFOR)
endif

LINKERSCRIPT = kernel.ld

KERNEL = $(BINDIR)/kernel

OUTPUTS = $(KERNEL)

NEEDSMALLEST :=

LDFLAGS ?=  --oformat elf64-x86-64 -no-stdlib -M -T kernel.ld

#-m i386linux

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.c=.o): CFLAGS += -Os -DNDEBUG

all: $(OUTPUTS) $(DUMPDIR)/kernel-headers

clean:
	@rm -f $(BINDIR)/kernel
	@rm -f $(OBJS)
	@rm -f $(DUMPDIR)/*.map
	@rm -f $(OUTPUTS)

$(KERNEL): $(OBJS) $(LINKERSCRIPT)
	ld -melf_x86_64 $(LDFLAGS) $(OUTPUT_OPTION) $(OBJS) > $(DUMPDIR)/kernel.map

$(DUMPDIR)/kernel-headers: $(KERNEL)
	objdump --all-headers $< > $@

.PHONY: all clean

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

# Generate assembly dump for C
$(DUMPDIR)/%.s : %.c
$(DUMPDIR)/%.s : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) -S $<
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

# Tolerate the dependency files being missing
$(DEPDIR)/%.d: ;

# Prevent deletion of generated dependencies
.PRECIOUS: $(DEPDIR)/%.d

# Include the generated makefile fragments
-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(CSRCS)))
