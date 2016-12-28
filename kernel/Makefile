# Make kernel

BUILDROOT := $(realpath .)
export BUILDROOT

CONFIG ?= x86_64

include config/$(CONFIG).mk

CSRCS := \
	main.c

ASRCS :=

BINDIR := bin
DUMPDIR := dump

$(shell mkdir -p $(BINDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)

CC_IS_CLANG := $(shell ($(CC) -v 2>&1 | \
	head -n 1 | \
	grep clang > /dev/null) && \
	echo 1)

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.c=.o)

OBJS := $(AOBJS) $(COBJS)

TARGET_ARCH := -m64
TARGET_ARCH_AS := --64

INCS := arch arch/$(ARCH) lib
INCLUDEPATHS := $(realpath $(INCS))
INCLUDEARGS := $(patsubst %,-I%,$(INCLUDEPATHS))

# Max warnings
WEVERYTHING := -Wall
WEVERYTHING += -Wextra
WEVERYTHING += -Wpedantic
WEVERYTHING += -Werror
#too many false positives WEVERYTHING  = -Wconversion
WEVERYTHING += -Wdouble-promotion
WEVERYTHING += -Wformat=2
WEVERYTHING += -Wfloat-equal
WEVERYTHING += -Wundef
WEVERYTHING += -Wbad-function-cast
#overkill WEVERYTHING += -Wcast-qual
WEVERYTHING += -Wwrite-strings
WEVERYTHING += -Wmissing-prototypes
WEVERYTHING += -Wredundant-decls

ifneq ($(CC_IS_CLANG),1)
	WEVERYTHING += -Wunsuffixed-float-constants
endif

# Strict overflow is noise
WEVERYTHING += -Wno-strict-overflow

# Oh come on, interferes with inline assembly
WEVERYTHING += -Wno-overlength-strings

ifndef TLS_MODEL
TLS_MODEL := local-exec
endif

CFLAGS ?= -g3 -fno-stack-protector \
	-fpic -mno-red-zone \
	-msse -msse2 \
	-ftls-model=$(TLS_MODEL) \
	-fexceptions -fno-common  \
	$(WEVERYTHING)

#OPTIMIZE := 1

ifdef OPTIMIZE
CFLAGS += \
	-O2 -ftree-vectorize \
	-fomit-frame-pointer -fbuiltin
else
CFLAGS += \
	-O0 \
	-fno-omit-frame-pointer
endif

#-fipa-icf

CFLAGS += $(INCLUDEARGS)

export CFLAGS

ASFLAGS ?= --warn

ifdef TUNEFOR
	CFLAGS += -mtune=$(TUNEFOR)
endif

LIBGCC := $(shell $(CC) -print-libgcc-file-name)
LIBGCCDIR := $(dir $(LIBGCC))
LIBGCCFILENAME = $(notdir $(LIBGCC))
LIBGCCNAME := $(patsubst lib%.a,%,$(LIBGCCFILENAME))

LINKERSCRIPT := arch/$(ARCH)/kernel.ld
KERNEL := $(BINDIR)/kernel

LIBFILES := \
	lib/libkernel.a \
	arch/$(ARCH)/libarch.a

OUTPUTS = $(KERNEL)

NEEDSMALLEST :=

LDFLAGS ?=  --oformat elf64-x86-64 -no-stdlib -M -T $(LINKERSCRIPT) -g

#-m i386linux

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.c=.o): CFLAGS += -Os -DNDEBUG

all: subdirs $(OUTPUTS) $(DUMPDIR)/kernel-headers $(DUMPDIR)/kernel.dis

clean:
	@rm -f $(BINDIR)/kernel
	@rm -f $(OBJS)
	@rm -f $(DUMPDIR)/*.map
	@rm -f $(OUTPUTS)
	(cd lib && $(MAKE) clean)
	(cd arch/$(ARCH) && $(MAKE) clean)

subdirs:
	+(cd lib && $(MAKE))
	+(cd arch/$(ARCH) && $(MAKE))

$(LIBFILES): subdirs

.PHONY: subdirs

$(KERNEL): $(OBJS) $(LINKERSCRIPT) $(LIBFILES)
	ld -melf_x86_64 $(LDFLAGS) $(OUTPUT_OPTION) $(OBJS) \
		--start-group $(LIBFILES) --end-group \
		$(LIBGCC) \
		> $(DUMPDIR)/kernel.map

$(DUMPDIR)/kernel-headers: $(KERNEL)
	objdump --all-headers $< > $@

$(DUMPDIR)/kernel.dis: $(KERNEL)
	objdump --disassemble --source $^ > $@

include autodep.mk
