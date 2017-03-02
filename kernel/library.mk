# Expects:
#  LIBNAME (name without lib prefix or .a suffix)
#  CSRCS
#  ASRCS
#  ...and everything autodep.mk expects

LIBFILENAME := lib$(LIBNAME).a

# Determine object file filenames
AOBJS := $(ASRCS:.s=.o)
COBJS := $(CSRCS:.c=.o)

SRCS := $(ASRCS) $(CSRCS)

OBJS := $(AOBJS) $(COBJS)

all: $(LIBFILENAME)

clean:
	$(RM) -f $(OBJS)
	$(RM) -f $(LIBFILENAME)
	if [ "$(CLEANOTHER)" != "" ]; then $(RM) $(CLEANOTHER); fi
	$(RM) -f $(patsubst %,$(DEPDIR)/%.d,$(notdir $(SRCS)))

$(LIBFILENAME): $(OBJS)
	$(AR) -rcs $@ $^

include $(BUILDROOT)/autodep.mk
