# Expects:
#  MODNAME (name without .km suffix)
#  CSRCS
#  ASRCS
#  ...and everything autodep.mk expects

CONFIG ?= x86_64

include $(BUILDROOT)/config/$(CONFIG).mk

MODFILENAME := $(MODNAME).km

# Determine object file filenames
AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.c=.o)

SRCS := $(ASRCS) $(CSRCS)

OBJS := $(AOBJS) $(COBJS)

LINKERSCRIPT ?= $(BUILDROOT)/arch/$(ARCH)/module.ld

KERNELINCLUDES := arch/$(ARCH) lib net $(BUILDROOT)
INCS := $(INCS) $(patsubst %,$(BUILDROOT)/%,$(KERNELINCLUDES))
INCLUDEPATHS := $(realpath $(INCS))
INCLUDEARGS := $(patsubst %,-I%,$(INCLUDEPATHS))
CFLAGS := $(CFLAGS) $(INCLUDEARGS)

$(info $(INCLUDEPATHS))

all: $(MODFILENAME)

clean:
	@$(RM) -f $(OBJS)
	@$(RM) -f $(MODFILENAME)
	if [ "$(CLEANOTHER)" != "" ]; then $(RM) $(CLEANOTHER); fi
	@$(RM) -f $(patsubst %,$(DEPDIR)/%.d,$(notdir $(SRCS)))

$(MODFILENAME): $(OBJS) $(LINKERSCRIPT) $(BUILDROOT)/module.mk
	$(LD) -shared -melf_x86_64 -z max-page-size=4096 \
		$(LDFLAGS) -T $(LINKERSCRIPT) \
		-o $@ \
		$(OBJS)

include $(BUILDROOT)/autodep.mk
