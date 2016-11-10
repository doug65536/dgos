# Make kernel

BUILDROOT = $(realpath .)
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
WEVERYTHING += -Wunsuffixed-float-constants

# Strict overflow is noise
#WEVERYTHING += -Wno-strict-overflow

ifndef TLS_MODEL
TLS_MODEL := local-exec
endif

CFLAGS ?= -g3 -O0 -ffreestanding \
	-Wno-unused-function -fpic -mno-red-zone \
	-ftls-model=$(TLS_MODEL) \
	$(WEVERYTHING)

CFLAGS += $(INCLUDEARGS)

export CFLAGS

ASFLAGS ?= --warn

#-mcmodel=large

ifdef TUNEFOR
	CFLAGS += -mtune=$(TUNEFOR)
endif

LINKERSCRIPT := kernel.ld

KERNEL := $(BINDIR)/kernel

LIBFILES := lib/libkernel.a \
	arch/$(ARCH)/libarch.a

OUTPUTS = $(KERNEL)

NEEDSMALLEST :=

LDFLAGS ?=  --oformat elf64-x86-64 -no-stdlib -M -T kernel.ld -g

#-m i386linux

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.c=.o): CFLAGS += -Os -DNDEBUG

all: libs $(OUTPUTS) $(DUMPDIR)/kernel-headers $(DUMPDIR)/kernel.dis

clean:
	@rm -f $(BINDIR)/kernel
	@rm -f $(OBJS)
	@rm -f $(DUMPDIR)/*.map
	@rm -f $(OUTPUTS)
	(cd lib && $(MAKE) clean)
	(cd arch/$(ARCH) && $(MAKE) clean)

$(LIBFILES) libs:
	(cd lib && $(MAKE))
	(cd arch/$(ARCH) && $(MAKE))

.PHONY: libs

$(KERNEL): $(LIBFILES)  $(OBJS) $(LINKERSCRIPT)
	ld -melf_x86_64 $(LDFLAGS) $(OUTPUT_OPTION) $(OBJS) \
		--start-group $(LIBFILES) --end-group \
		> $(DUMPDIR)/kernel.map

$(DUMPDIR)/kernel-headers: $(KERNEL)
	objdump --all-headers $< > $@

$(DUMPDIR)/kernel.dis: $(KERNEL)
	objdump --disassemble --source $^ > $@

include autodep.mk
