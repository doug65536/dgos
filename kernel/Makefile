# Make kernel

BUILDROOT := $(realpath .)
export BUILDROOT

CONFIG ?= x86_64

include config/$(CONFIG).mk

CSRCS := \
	main.cc

ASRCS :=

BINDIR := bin
DUMPDIR := dump

$(shell mkdir -p $(BINDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)

OPTIMIZE := 1
#LTO := 1
CC := clang
#CC := gcc-6

export CC

CC_IS_CLANG := $(shell ($(CXX) -v 2>&1 | \
	head -n 1 | \
	grep clang > /dev/null) && \
	echo 1)

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.cc=.o)

OBJS := $(AOBJS) $(COBJS)

TARGET_ARCH := -m64
TARGET_ARCH_AS := --64

INCS := . arch arch/$(ARCH) lib fs net extern/include
INCLUDEPATHS := $(patsubst %,$(BUILDROOT)/%,$(INCS))
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
#WEVERYTHING += -Wbad-function-cast
#overkill WEVERYTHING += -Wcast-qual
WEVERYTHING += -Wwrite-strings
#WEVERYTHING += -Wmissing-prototypes
WEVERYTHING += -Wredundant-decls
WEVERYTHING += -Wshadow

# Strict overflow is noise
WEVERYTHING += -Wno-strict-overflow

# Oh come on, interferes with inline assembly
WEVERYTHING += -Wno-overlength-strings

ifndef TLS_MODEL
TLS_MODEL := local-exec
endif

CXXFLAGS := -g3 -fno-stack-protector \
	-mcmodel=kernel -mno-red-zone \
	-msse -msse2 \
	-fno-exceptions -fno-common \
	-fvisibility=hidden \
	-std=c++14 -fpermissive -ffreestanding -fno-rtti \
	$(WEVERYTHING)

#-std=c++11 -x c++ \

ifdef OPTIMIZE
CXXFLAGS += \
	-O2 -ftree-vectorize \
	-fno-omit-frame-pointer -fbuiltin -fno-builtin-ilogbl
else
CXXFLAGS += \
	-O0 \
	-fno-omit-frame-pointer
endif
#-march=native
RANLIB := gcc-ranlib-6
AR := gcc-ar-6
NM := gcc-nm-6
export RANLIB
export AR
export NM

ifdef LTO
CXXFLAGS += -flto=8
endif

CXXFLAGS += $(OPTIMIZATIONFLAGS)

#-fipa-icf

CXXFLAGS += $(INCLUDEARGS)

export CXXFLAGS

ifdef TUNEFOR
	CXXFLAGS += -mtune=$(TUNEFOR)
endif

LIBGCC := $(shell $(CXX) -print-libgcc-file-name)
LIBGCCDIR := $(dir $(LIBGCC))
LIBGCCFILENAME = $(notdir $(LIBGCC))
LIBGCCNAME := $(patsubst lib%.a,%,$(LIBGCCFILENAME))

LINKERSCRIPT := arch/$(ARCH)/kernel.ld
KERNEL := $(BINDIR)/kernel

LIBFILES := \
	lib/libkernel.a \
	arch/$(ARCH)/libarch.a

EXTERNLIBFILES := \
	extern/zlib-src/build/libz.a

LIBFILES_WHOLE := \
	fs/libfs.a \
	net/libnet.a

OUTPUTS = $(KERNEL)

NEEDSMALLEST :=

LD := ld

LDFLAGS ?=  --oformat elf64-x86-64 \
	-Map $(DUMPDIR)/kernel.map \
	-T $(LINKERSCRIPT) \
	-g -export-dynamic \
	-z max-page-size=4096

# Add smallest code optimization to all objects for sources in NEEDSMALLEST
$(NEEDSMALLEST:.cc=.o): CFLAGS += -Os -DNDEBUG

all: subdirs $(EXTERNLIBFILES) $(OUTPUTS) $(DUMPDIR)/kernel-headers $(DUMPDIR)/kernel.dis

clean:
	@rm -f $(BINDIR)/kernel
	@rm -f $(OBJS)
	@rm -f $(DUMPDIR)/*.map
	@rm -f $(OUTPUTS)
	@rm -rf extern/include $(EXTERNLIBFILES)
	(cd lib && $(MAKE) clean)
	(cd net && $(MAKE) clean)
	(cd fs && $(MAKE) clean)
	(cd arch/$(ARCH) && $(MAKE) clean)

distclean: clean
	@rm -rf extern

subdirs: $(EXTERNLIBFILES)
	+(cd lib && $(MAKE))
	+(cd net && $(MAKE))
	+(cd fs && $(MAKE))
	+(cd arch/$(ARCH) && $(MAKE))

$(LIBFILES): subdirs

.PHONY: subdirs

WLPREFIX := -Wl,

$(KERNEL): $(OBJS) $(LINKERSCRIPT) $(LIBFILES) $(EXTERNLIBFILES) $(LIBFILES_WHOLE)
	$(CXX) $(CXXFLAGS) $(OUTPUT_OPTION) \
		-fuse-linker-plugin \
		-nostdlib -Wl,-melf_x86_64 \
		$(patsubst %,$(WLPREFIX)%,$(LDFLAGS)) \
		$(OBJS) \
		-Wl,--start-group \
		$(LIBFILES) \
		$(EXTERNLIBFILES) \
		-Wl,--whole-archive $(LIBFILES_WHOLE) -Wl,--no-whole-archive \
		-Wl,--end-group \
		-Wl,-build-id=none

#$(LIBGCC) \

$(DUMPDIR)/kernel-headers: $(KERNEL)
	objdump --all-headers $< > $@

$(DUMPDIR)/kernel.dis: $(KERNEL)
	objdump --disassemble --source $^ > $@

include dep-zlib.mk

include autodep.mk
