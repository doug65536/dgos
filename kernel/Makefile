# Make kernel

BUILDROOT := $(realpath .)
export BUILDROOT

ifndef CONFIG
	CONFIG := x86_64
endif

include config/$(CONFIG).mk

CSRCS := \
	main.cc

ASRCS :=

BINDIR := bin
DUMPDIR := dump

$(shell mkdir -p $(BINDIR) >/dev/null)
$(shell mkdir -p $(DUMPDIR) >/dev/null)

OPTIMIZE := 1
OPTIMIZE_AVX := 1
#LTO := 1
#CC := clang
#CXX := clang++
CC := gcc-6
CXX := gcc-6

export CC
export CXX

CXX_IS_CLANG := $(shell ($(CXX) -v 2>&1 | \
	head -n 1 | \
	grep clang > /dev/null) && \
	echo 1)

AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.cc=.o)

OBJS := $(AOBJS) $(COBJS)

TARGET_ARCH := -m64
TARGET_ARCH_AS := --64

INCS := . arch arch/$(ARCH) lib lib/cc cc fs net extern/include
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

CXXFLAGS := -std=c++14 -g3 \
	-mcmodel=kernel -mno-red-zone \
	-fvisibility=hidden \
	-fno-stack-protector -fno-exceptions -fno-common -fno-rtti \
	-msse -msse2 \
	$(WEVERYTHING)

ifdef OPTIMIZE
CXXFLAGS += \
	-O2 -ftree-vectorize -mtune=native \
	-fno-omit-frame-pointer -fbuiltin -fno-builtin-ilogbl
#CXXFLAGS += -DNDEBUG

ifdef OPTIMIZE_AVX
CXXFLAGS += -mavx -mavx2

ifdef CXX_IS_CLANG
CXXFLAGS += -no-integrated-as
endif

endif
# -Wa,-msse2avx
else
CXXFLAGS += \
	-O0 \
	-fno-omit-frame-pointer
endif

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
	lib/libkernel.a

EXTERNLIBFILES := \
	extern/zlib-src/build/libz.a

LIBFILES_WHOLE := \
	arch/$(ARCH)/libarch.a \
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
$(NEEDSMALLEST:.cc=.o): CXXFLAGS += -Os -DNDEBUG

all: subdirs $(EXTERNLIBFILES) $(OUTPUTS) \
	$(DUMPDIR)/kernel-headers $(DUMPDIR)/kernel.dis

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

subdirs: $(EXTERNLIBFILES) Makefile
	+(cd lib && $(MAKE))
	+(cd net && $(MAKE))
	+(cd fs && $(MAKE))
	+(cd arch/$(ARCH) && $(MAKE))

$(LIBFILES): subdirs

.PHONY: subdirs

WLPREFIX := -Wl,

$(KERNEL): $(OBJS) $(LINKERSCRIPT) $(LIBFILES) \
		$(EXTERNLIBFILES) $(LIBFILES_WHOLE)
	mkdir -p $(DUMPDIR)
	$(CXX) $(CXXFLAGS) $(OUTPUT_OPTION) \
		-fuse-linker-plugin \
		-nostdlib -Wl,-melf_x86_64 \
		$(patsubst %,$(WLPREFIX)%,$(LDFLAGS)) \
		$(OBJS) \
		-Wl,--start-group \
			$(LIBFILES) \
			$(EXTERNLIBFILES) \
			-Wl,--whole-archive \
				$(LIBFILES_WHOLE) \
			-Wl,--no-whole-archive \
		-Wl,--end-group \
		-Wl,-build-id=none

#$(LIBGCC) \

$(DUMPDIR)/kernel-headers: $(KERNEL)
	mkdir -p $(@D)
	objdump --all-headers $< > $@

$(DUMPDIR)/kernel.dis: $(KERNEL)
	mkdir -p $(@D)
	objdump --disassemble --source $^ > $@

include dep-zlib.mk

include autodep.mk
